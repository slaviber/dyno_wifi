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
*   logic.c implements the back-end logic of the Process Controller.
*   Except for the PID control functions, which are in a separate file.
*
*/

#include "logic.h"
#include "pid.h"
#include "soc/gpio_struct.h" // FVCKING 3.14 bug
#include <math.h>
#include <driver/mcpwm_timer.h>

// The floating-point registers
static DRAM_ATTR uint32_t cp0_regs[18];

static float calc_kmh(_Atomic int *kmh_pv);
static float calc_n10(_Atomic int *n10_pv);
static float calc_acc(float kmh, float kmh_hist[4]);
static bool input_blackman_filter(blackman_state *bs);
static void set_dds_output(int ppm, uint8_t dither_val);
static int brake_comp(int val);
static float sweep_get();

#define CPN10(val) calc_percent(hwdesc.adc_min, hwdesc.adc_range_inv, val)
#define CPKMH(val) calc_percent(hwdesc.pulse_min, hwdesc.pul_range_inv, val)
// #define RPN10(val) rev_percent(hwdesc.adc_min, hwdesc.adc_max, val)

// pin4, pin5 work as inputs!
static char DRAM_ATTR oflow_uflow_pulse_flag[N_INPUTS] = {0};

// NOTE: N_HOLES is 12, the input frequency is /2, but we're measuring BOTH levels!
#define N_HOLES 12
static DRAM_ATTR bool one_delay[N_INPUTS] = {
    false}; // ignore the first value after an overflow
static DRAM_ATTR int8_t hole_delay[N_INPUTS] = {
    N_HOLES}; // wait for a complete revolution(mv avg)
static DRAM_ATTR uint16_t rpm_circ_buf[N_HOLES][N_INPUTS] = {};
static DRAM_ATTR int16_t pcnt_rpm_val[N_INPUTS];

void update_bridge_state() { // Update in the network thread, save cpu cycles in the dyno task /:

  // Using the slow filter
  float kmhbr = calc_kmh(&(graph_filter.outputs)[consts.kmh_brake]);
  float kmhid = calc_kmh(&(graph_filter.outputs)[consts.kmh_idler]);

  if ((kmhbr < 5.0f) && (kmhid < 5.0f)) {
    pc_state[LIFT_OUT] = cnt_seti.lb_state;
    if (cnt_seti.lb_state == LB_DN) {
      gpio_set_level(LIFT_OUT_GPIO, 0);
    } else {
      gpio_set_level(LIFT_OUT_GPIO, 1);
    }
  } else { // speed is too high, failsafe
    gpio_set_level(LIFT_OUT_GPIO, LB_DN);
    pc_state[LIFT_OUT] = LB_DN;
  }
}

/* When HIGH level interrupt is triggered, returns TRUE */
static inline bool IRAM_ATTR mitigate_314(gpio_num_t gpio) {
  int fvcking_lvl = 0; // DAMN FVCKING ERRATA 3.14  SH!T
  if (gpio < 32) {
    fvcking_lvl = (GPIO.in >> gpio) & 0x1;
  } else {
    fvcking_lvl = ((GPIO.in1.data) >> (gpio - 32)) & 0x1;
  }

  // int fvcking_lvl = gpio_get_level(gpio); // DAMN FVCKING ERRATA 3.14  SH!T
  if (fvcking_lvl) { // level is HIGH and we operate only on LOW level
    GPIO.pin[gpio].int_type = GPIO_INTR_LOW_LEVEL;
    return true;
  }
  GPIO.pin[gpio].int_type = GPIO_INTR_HIGH_LEVEL;
  return false;
}

void go_to_failsafe() {
  set_dds_output(hwdesc.dac_unload * 100, 0); // UNLOAD BRAKE, in ppm
  cnt_seti.mop = MOP_END;
  current_frq = hwdesc.dac_unload;
  sws_reset();
  lock_reset();
  set_dds_output(hwdesc.dac_unload * 100, 0); // UNLOAD BRAKE, in ppm
}

void ADC_isr_handler(pcnt_isr_ctx *ctx) {
  if (mitigate_314(ctx->control)) return;

  pcnt_unit_get_count(ctx->unit,
                      (int *)ctx->value); // TODO: Is the cast a problem?
  pcnt_unit_clear_count(ctx->unit);
}

bool pulse_ouflow_handler(pcnt_unit_handle_t unit,
                          const pcnt_watch_event_data_t *edata,
                          void *user_ctx) {
  (void)unit; // unit check?
  (void)edata;
  pcnt_isr_ctx *ctx = user_ctx;
  // PCNT.int_clr.val = 0xFF;
  oflow_uflow_pulse_flag[ctx->input_nr] = 1;
  ctx->value[0] = 10000000; // very large number / slow speed
  return false;
}

__attribute__((always_inline)) static inline uint8_t inc_RCP(unsigned short N) {
  // static uint8_t rpm_circ_ptr;
  static uint32_t rpm_circ_ptr
      [N_INPUTS]; // FIXME : THIS introduces a BUG !!! on overflow ! every 357 M rotations
  return (rpm_circ_ptr[N]++) % N_HOLES;
}

__attribute__((optimize("unroll-loops")))
__attribute__((always_inline)) static inline uint16_t
rpm_mv_avg(uint8_t N) {
  uint32_t sum = 0;
#pragma GCC unroll 100
  for (unsigned i = 0; i < N_HOLES; ++i) sum += rpm_circ_buf[i][N];
  return (uint16_t)(sum / N_HOLES * 2);
}

/* MAX 2us execution time at 160 MHz ? */
void Pulse_isr_handler(pcnt_isr_ctx *ctx) {
  pcnt_unit_handle_t unit = ctx->unit;
  if (mitigate_314(ctx->control)) // Low or High CS level
    // return;
    unit = ctx->unit_c; // Use the complementary unit

  uint8_t N = ctx->input_nr;

  int res = 0;
  pcnt_unit_get_count(unit, &res);
  pcnt_unit_clear_count(unit);
  pcnt_rpm_val[N] = (int16_t)res; // FIXME: This is useless?

  // BM_DAC(DAC_CHANNEL_2, (uint8_t)((res-10000)/20));

  no_rpm[N] = 0;

  if (oflow_uflow_pulse_flag[N]) {
    ctx->value[0] = 10000000; // very large number / slow speed
    oflow_uflow_pulse_flag[N] = 0;
    one_delay[N] = true;
  } else {
    rpm_circ_buf[inc_RCP(N)][N] = pcnt_rpm_val[N];
    if (one_delay[N]) {
      one_delay[N] = false;
      hole_delay[N] = N_HOLES;
    } else {
      if (hole_delay[N] > 0) hole_delay[N]--;
      else ctx->value[0] = rpm_mv_avg(N);
    }
  }
}

void IRAM_ATTR sws_reset() {
  // Terminate the sweep
  sws.sweep_run = false;
  // Reset the mop
  sws.sweep_mode = MOP_END;
  // Discard the buffers
  sws.remaining_size[0] = 0;
  sws.remaining_size[1] = 0;
}

void IRAM_ATTR lock_reset() {
  // reset the locks
  pc_state[LOCK_PID1] = false;
  pc_state[LOCK_PID2] = false;
}

int IRAM_ATTR sweep_top(float *data, int len) {
  // Wrong length
  if (!len || len > hwdesc.buflen) return REST_BAD;
  // Wrong mop or forced reset
  if (sws.sweep_mode >= MOD_SWEEP) return REST_BAD;
  // The idle side
  bool side = !sws.buffer_side;
  // Prevent race-conditions, pepare for a swap
  if (sws.remaining_size[!side] < 2 && true == sws.sweep_run) return REST_AGAIN;
  // Fill both sides on startup
  if (false == sws.sweep_run) side = !side;
  // No space left in buffer
  if (sws.remaining_size[side]) return REST_AGAIN;
  else {
    int bufpos = hwdesc.buflen - len;
    int buflen = len * sizeof(float);
    // Put the data at the end of the buffer
    memcpy(&sws.sweep_buf[side][bufpos], data, buflen);
    sws.remaining_size[side] = len;
    // Start the sweep
    if (false == sws.sweep_run) {
      // TODO: This is needed, right?
      kmh_change = true;
      n10_change = true;
      sws.sweep_run = true;
    }
    return REST_OK;
  }
  return REST_OK;
}

static float IRAM_ATTR sweep_get() {
  float num = NAN;
  // Not running
  if (!sws.sweep_run) return num;
  // The active side
  bool side = sws.buffer_side;
  // Flip side, if needed
  if (!sws.remaining_size[side]) side = sws.buffer_side = !side;
  // Empty buffer
  if (!sws.remaining_size[side]) num = NAN;
  else {
    int bufpos = hwdesc.buflen - sws.remaining_size[side]--;
    num = sws.sweep_buf[side][bufpos];
  }
  // No more data left in buffer
  if (isnanf(num)) {
    sws_reset();
    lock_reset();
  }
  return num;
}

static float IRAM_ATTR calc_kmh(_Atomic int *kmh_pv) {
  int rpmfilt = *kmh_pv;
  if (rpmfilt > RPM_MAX) return 0;   // failsafe
  if (rpmfilt < RPM_MIN) return 300; // failsafe
  int rpms = rpmfilt - cnt_seti.kmh_off;
  float kmhs = (float)cnt_seti.kmh_div / (float)rpms;
  // Perform input clipping
  if (kmhs < hwdesc.pulse_min) kmhs = hwdesc.pulse_min;
  if (kmhs > hwdesc.pulse_max) kmhs = hwdesc.pulse_max;
  return kmhs;
}

static float IRAM_ATTR calc_n10(_Atomic int *n10_pv) {
  int capts = *n10_pv - cnt_seti.n10_off;
  float n10s = (float)capts / (float)cnt_seti.n10_div;
  // Perform input clipping
  if (n10s < hwdesc.adc_min) n10s = hwdesc.adc_min;
  if (n10s > hwdesc.adc_max) n10s = hwdesc.adc_max;
  //TODO: FIXME: change the DIV coefficients to actual MUL coefficients :/ with enough precision :(((
  return n10s;
}

static float IRAM_ATTR calc_acc(float kmh, float kmh_hist[4]) {
  for (int i = 0; i < 3; i++) {
    kmh_hist[i] = kmh_hist[i + 1];
  }
  kmh_hist[3] = kmh;
  /* mult f/(6*3.6)[11 18 9 2] for 119.0476 Hz, input in km/h, output in m/s^2 */
  /* Third-order approximate first derivative, BDF3 */
  return hwdesc.pid_freq *
         (0.5092592f * kmh_hist[3] - 0.8333333f * kmh_hist[2] +
          0.4166666f * kmh_hist[1] - 0.0925925f * kmh_hist[0]);
}

static bool IRAM_ATTR input_blackman_filter(blackman_state *bs) {
  int n_streams = bs->nr_streams;
  float(*f_state)[n_streams + 2] = bs->filter_state;
  int *f_counter = &bs->fs_counter;
  int s_length = bs->stream_length;
  for (int i = 0; i < N_INPUTS; ++i) {
    atomic_int *val = NULL;
    switch (input_arr[i]) {
    case INPUT_ADC:
      val = &captured[i];
      break;
    case INPUT_PULS:
      val = &rpm[i];
      break;
    case INPUT_NONE:
      continue;
      break;
    }
    for (int j = 0; j < n_streams; j++) {
      f_state[i][1 + j] += bs->filter_coefs[*f_counter + s_length * j] * (*val);
    }
  }
  (*f_counter)++;
  if (!((*f_counter) % s_length)) {
    (*f_counter) = 0;
    for (int i = 0; i < N_INPUTS; i++) {
      memmove(f_state[i] + 1, f_state[i],
              sizeof(f_state[i][0]) * (n_streams + 1));        // shift by 1
      bs->outputs[i] = (int)roundf(f_state[i][n_streams + 1]); // the output
    }
    return true;
  }
  return false;
}

static bool IRAM_ATTR gp_blackman_filter(blackman_state *bs, atomic_int *in,
                                         int n_inputs) {
  int n_streams = bs->nr_streams;
  float(*f_state)[n_streams + 2] = bs->filter_state;
  int *f_counter = &bs->fs_counter;
  int s_length = bs->stream_length;
  for (int i = 0; i < n_inputs; ++i) {
    for (int j = 0; j < n_streams; j++) {
      f_state[i][1 + j] +=
          bs->filter_coefs[(*f_counter) + s_length * j] * in[i];
    }
  }
  (*f_counter)++;
  if (!((*f_counter) % s_length)) {
    *f_counter = 0;
    for (int i = 0; i < n_inputs; i++) {
      memmove(f_state[i] + 1, f_state[i],
              sizeof(f_state[i][0]) * (n_streams + 1));        // shift by 1
      bs->outputs[i] = (int)roundf(f_state[i][n_streams + 1]); // the output
    }
    return true;
  }
  return false;
}

bool timer_alarm_cb(gptimer_handle_t timer,
                    const gptimer_alarm_event_data_t *edata,
                    void *user_data) { // 10 kHz rate
  // get FPU state
  uint32_t cp_state = xthal_get_cpenable();
  if (cp_state) {
    // Save FPU registers
    xthal_save_cp0(cp0_regs);
  } else {
    // enable FPU
    xthal_set_cpenable(1);
  }

  //The setpoint + the wind resistance
  static float n10_target = 0;
  static float kmh_target = 0;
  static float acc_target = 0;

  // every 63`000 ticks the three filters superimpose!
  if (input_blackman_filter(&pid_filter)) { // (119.0476 Hz), every 750=megasup

    for (int i = 0; i < puls_idx; ++i) {
      int N =
          puls_arr[i]; // the input number that is assigned to this pulse input
      if (N < 0) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
      no_rpm[N]++;
      if (no_rpm[N] > 10) { // No rpm input from sensor!
        oflow_uflow_pulse_flag[N] = 1;
        rpm[N] = 10000000; // very large number / slow speed
      }
    }

    static float DRAM_ATTR kmh1_hist[4] = {0.0f};
    // static float DRAM_ATTR kmh2_hist[4] = {0.0f};

    // The brake input
    float force =
        calc_n10(&(pid_filter.outputs)[consts.adc_force]); // fast fltd
    // The brake roller
    float kmh1 = calc_kmh(&(pid_filter.outputs)[consts.kmh_brake]); // fast fltd
    float acc_in = calc_acc(kmh1, kmh1_hist); // This is third-order !
    // The idler roller
    float kmh2 = calc_kmh(&(pid_filter.outputs)[consts.kmh_idler]); // fast fltd
    // float accel2 = calc_acc(kmh2, kmh2_hist); // This is third-order !

    // Do not permit slipping !!!
    float slip = fabsf(kmh2 - kmh1) - cnt_seti.slk;
    if (slip > 0.f) pc_state[SLIP_FLAG] = true;
    else {
      pc_state[SLIP_FLAG] = false;
      slip = 0.f;
    }

    n10_target = cnt_seti.target_n10;
    kmh_target = cnt_seti.target_kmh;
    acc_target = 0.f;

    int mopval = cnt_seti.mop;
    if (MOD_SWEEP == mopval) mopval = sws.sweep_mode;
    switch (mopval) {
    case CONST_KMH:
      // Cascaded PID
      acc_target = kmh_pid(kmh1, kmh_target);
      acc_pid(kmh1, acc_in, slip, acc_target);
      break;
    case CONST_N10:
      // Add drag in n10 mode
      float mps1 = 0.277777777f * kmh1; // km/h to m/s
      n10_target =
          cnt_seti.target_n10 + drag_coef * mps1 * mps1 * 0.15915494f * whr_inv;
      n10_pid(kmh1, CPN10(force), slip, CPN10(n10_target));
      break;
    case CONST_FRQ:
      current_frq = cnt_seti.target_frq;
      break;
    default:                           /* (MOD_SWEEP; MOP_END) */
      current_frq = hwdesc.dac_unload; // Keep the pc unloaded
      break;
    }
  }
  if (input_blackman_filter(&graph_filter)) { // 20 Hz, every 126=megasup
    if (sws.sweep_run) {
      float val = sweep_get(); // Extract the next setpoint
      if (!isnanf(val)) {
        switch (sws.sweep_mode) {
        case CONST_KMH:
          if (val < hwdesc.pid2_min || val > hwdesc.pid2_max) go_to_failsafe();
          cnt_seti.target_kmh = val;
          break;
        case CONST_N10:
          if (val < hwdesc.pid1_min || val > hwdesc.pid1_max) go_to_failsafe();
          cnt_seti.target_n10 = val;
          break;
        case CONST_FRQ:
          if (val < hwdesc.dac_min || val > hwdesc.dac_max) go_to_failsafe();
          cnt_seti.target_frq = (int)roundf(val);
          break;
        default:
          go_to_failsafe();
          break;
        }
      }
    }
    // Status update
    pc_state[SWP_STATE] = sws.sweep_run;
    current_prg = sws.remaining_size[0] + sws.remaining_size[1];

    // The brake input
    float force_smooth =
        calc_n10(&(graph_filter.outputs)[consts.adc_force]); // slow fltd
    // The brake roller
    float speed_smooth =
        calc_kmh(&(graph_filter.outputs)[consts.kmh_brake]); // slow fltd

    // 1% is tolerable ?!
    if (cnt_seti.mop == CONST_N10 ||
        (cnt_seti.mop == MOD_SWEEP && sws.sweep_mode == CONST_N10))
      pc_state[LOCK_PID1] =
          (fabsf(CPN10(n10_target) - CPN10(force_smooth)) < 1.f);

    if (cnt_seti.mop == CONST_KMH ||
        (cnt_seti.mop == MOD_SWEEP && sws.sweep_mode == CONST_KMH))
      pc_state[LOCK_PID2] =
          (fabsf(CPKMH(kmh_target) - CPKMH(speed_smooth)) < 1.f);

    // This is used for logical feedback only, to keep the UI updated in real time!
    dyn_state[0] = cnt_seti.mop;
    // dyn_state[1] = cnt_seti.target_frq;
    dyn_state[1] = current_frq;
    dyn_state[2] = n10_target;
    dyn_state[3] = kmh_target;
    dyn_state[4] = acc_target;
  }
  atomic_int gp_state[2];
  gp_state[0] = current_frq;
  gp_state[1] = current_prg;
  gp_blackman_filter(&gp_graph_filter, gp_state, 2); // 20 Hz, every 126=megasup

  dither_counter++; // Used in ADC dithering !!!!! DONOTTOUCH
  if (!(dither_counter % 8)) dither_counter = 0; // Once in a while

  // out 6W to 1.4 kW // NEVER, EVER, EVER drive the output below 10 kHz !!!!! No dithering, bad output accuracy and ALIASING !!!!!!
  set_dds_output(brake_comp(current_frq), dither_counter);
  if (cp_state) {
    // Restore FPU registers
    xthal_restore_cp0(cp0_regs);
  } else {
    // turn it back off
    xthal_set_cpenable(0);
  }

  return false;
}

static int IRAM_ATTR brake_comp(int val) {     // FLOAT !!!!!
  float tW = (0.000155f * val - 0.05f) * 1e3f; // theoretical W+0.1 !!!
  // float kW = (-242.169e-6*tW*tW + 1.328*tW - 53.057)/1e3; // sw compensation 1/real output 14.5 deg C ???
  // float kW = (-311.877e-6 * tW * tW + 1.445 * tW - 49.579) / 1e3; // sw compensation 1/real output 16 deg C
  // float kW = (-506.096e-6 * tW * tW + 1.676 * tW + 39.202) / 1e3; // sw compensation 1/real output 5 deg C
  // float kW = (-612.639e-6 * tW * tW + 1.854 * tW - 30.028) / 1e3; // sw compensation 1/real output ?? below 5 deg C
  // float kW = (-183e-6 * tW * tW + 1.234 * tW - 56.468) / 1e3; // sw compensation 1/real output ?? 1 deg c AFTER HW TEMP COMP
  float kW =
      (-215.988e-6f * tW * tW + 1.282f * tW - 62.414f) /
      1e3f; // sw compensation 1/real output ?? 10 deg c AFTER HW TEMP COMP 2
  // float kW = tW/1e3; // NON-COMPENSATED
  float kW2 = kW * kW;
  float kW3 = kW2 * kW;
  float kW4 = kW3 * kW;
  float kW5 = kW4 * kW;
  float kW6 = kW5 * kW;
  float kW7 = kW6 * kW;
  float kW8 = kW7 * kW;
  float kW9 = kW8 * kW;
  float target_vin_27deg = 1.9074f * kW9 - 13.93927f * kW8 + 43.77068f * kW7 -
                           77.22271f * kW6 + 83.97634f * kW5 - 58.07446f * kW4 +
                           25.46714f * kW3 - 6.89868f * kW2 + 1.20961f * kW -
                           0.12445633f;
  return (int)roundf(target_vin_27deg * 2711000.0f + 591165.f);
  // return val*100;
}

static void IRAM_ATTR set_dds_output(int ppm, uint8_t dither_val) {
  if (ppm > 1000000) ppm = 1000000;
  if (ppm < 0) ppm = 0;
  ppm = 1000000 - ppm; // the actual DAC value is inverted

  // uint64_t sum = 1024 * (uint64_t)ppm; // 10 bits
  // uint16_t dds_freq = 1024 + (uint32_t)(sum / 1000000);

  // uint64_t sum2 = 8192 * (uint64_t)ppm; // 3 bits more
  // uint16_t dds_freq2 = 8192 + (uint32_t)(sum2 / 1000000);
  // dds_freq2 = dds_freq2 & 0b111; // only the last 3 bits

  uint64_t sum = 13312 * (uint64_t)ppm; // 13.7 bits
  uint16_t dds_freq =
      6656 + 170 +
      (uint32_t)(sum /
                 1000000); // TODO !!!!!!!!!!! MAKE THIS OFFSET(170) SETTABLE THROUGH THE SETTINGS TABLE !!!!
  uint16_t dds_base = dds_freq >> 3, dds_dither = dds_freq & 0b111;

  if (dither_val < dds_dither) mcpwm_timer_set_period(adc_timer, dds_base + 1);
  else mcpwm_timer_set_period(adc_timer, dds_base + 0);
}

void blockingStatusChaining() { // Propagate the UART status chain
  static char line[MAX_STATUS_LINE_LENGTH];
  int size;
  char *ptr = line;
  while (1) {
    size = uart_read_bytes(UART_NUM_1, (unsigned char *)ptr, 1, portMAX_DELAY);
    if ((ptr - line) > MAX_STATUS_LINE_LENGTH - 1)
      break; // Do not allow line overflows
    if (size == 1) {
      if (*ptr == '\n') {
        uart_write_bytes(UART_NUM_0, line,
                         ptr - line + 1); // Output to the console
        break;
      }
      ptr++;
    }
  }
}
