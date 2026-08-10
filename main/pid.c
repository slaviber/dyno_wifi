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
*   pid.c contains the PID control function definitions.
*
*/

#include "data.h"
#include <math.h>

void n10_pid(float kmh, float n10_pct, float slip, float target_pct) {
  static float DRAM_ATTR prev_en10 = 0.0f;
  static float DRAM_ATTR intg_en10 = 0.0f;
  static float DRAM_ATTR diff_en10 = 0.0f;

  static bool DRAM_ATTR reset_int = false;
  static bool DRAM_ATTR reset_dir = false;

  if (target_pct < consts.pid1_min_pct) target_pct = consts.pid1_min_pct;
  if (target_pct > consts.pid1_max_pct) target_pct = consts.pid1_max_pct;

  // binary disable threshold
  static bool trip = false;
  if (kmh < 15.f) trip = true;
  if (kmh > 25.f) trip = false;

  if (trip) {
    intg_en10 = 0.0f;
    prev_en10 = n10_pct;
    current_frq = hwdesc.dac_min + cnt_seti.ff_pid1;
    return;
  }

  // Slip adds huge negative error in order to force brake unloading
  float en10 = target_pct - n10_pct - slip /**1.f */ * cnt_seti.s1m;

  intg_en10 += en10;
  // this is the first-order first derivative (BDF1) of the force
  // The derivative is independent of the set point !!
  diff_en10 = n10_pct - prev_en10;

  if (n10_change || mop_change) {
    n10_change = false;
    mop_change = false;
    // Reset only when the error is too big (> 7%)
    if (fabsf(en10) > 7.f) {
      reset_int = true;
      // if (target_#pct > n10_pct) reset_dir = true;
      reset_dir = (en10 > 0); // Reset on zero-crossing
      intg_en10 = 0.0f;       // Prevent windup
    }
  }
  /*(n10_pct >= target_#pct)*/
  // Reset when the new setpoint is crossed
  if (reset_int && reset_dir && en10 <= 0) {
    reset_int = false;
    intg_en10 = 0.0f; // Prevent setup overshoot
  }
  /*(n10_pct <= target_#pct)*/
  // Reset when the new setpoint is crossed
  if (reset_int && !reset_dir && en10 >= 0) {
    reset_int = false;
    intg_en10 = 0.0f; // Prevent setup overshoot
  }

  // FRQ_MIN - feedforward, lower values are unacceptable
  // PID in standard form
  float pid_val =
      cnt_seti.lp * hwdesc.dac_range *
          (en10 + cnt_seti.li * intg_en10 + cnt_seti.ld * diff_en10) +
      hwdesc.dac_min;

  if (pid_val < hwdesc.dac_min) pid_val = hwdesc.dac_min;
  if (pid_val > hwdesc.dac_max) pid_val = hwdesc.dac_max;

  current_frq = (int)roundf(pid_val + cnt_seti.ff_pid1);

  prev_en10 = n10_pct;
}

float kmh_pid(float kmh, float kmh_target) {

  if (kmh_target < hwdesc.pid2_min) kmh_target = hwdesc.pid2_min;
  if (kmh_target > hwdesc.pid2_max) kmh_target = hwdesc.pid2_max;

  // 1 m/s^2 of deceleration per every km/h diff:
  float kmh_err = kmh - kmh_target;
  // inner PID is P-only
  // Limit both the acceleration and the deceleration around the treshold in a linear fashion
  // lower coef means earlier response but slower, higher means delayed but faster :S
  // increasing BOTH the inner and outer P generally decreases response time !!!
  float target_acc = kmh_err * cnt_seti.p3p;
  // Max deceleration rate 30 m/s^2: (1 second 108->0 km/h !!! for a 30 km/h diff !!!)
  float max_acc = 30.0f * cnt_seti.tyre_traction;
  if (target_acc > max_acc) target_acc = max_acc;

  return target_acc;
}

void acc_pid(float kmh, float acc, float slip, float acc_target) {
  static float prev_eacc = 0.0f;
  static float intg_eacc = 0.0f;
  static float diff_eacc = 0.0f;

  if (mop_change) {
    mop_change = false;
    intg_eacc = 0;
  }

  float inr = cnt_seti.inr;
  static float last_I = 41.1f; // TODO: FIXME: HC :( CAN'T BECAUSE OF C :(
  if (last_I != inr) {
    intg_eacc *= last_I / inr; // TODO: FIXME: DIVISION ??!
    last_I = inr;
  }

  // This is PID2 error; cascaded PID
  // Error based on the target deceleration:
  // Slip adds huge negative error in order to force brake unloading
  float eacc = acc_target + acc - slip * hwdesc.pul_range_inv * cnt_seti.s2m;

  intg_eacc += eacc;
  // this is the first-order first derivative (BDF1) of the acceleration, the jerk
  // The derivative is independent of the set point !!
  diff_eacc = acc - prev_eacc;

  if (kmh_change) { // This statement should precede the next one
    kmh_change = false;
    if (!pc_state[LOCK_PID2]) intg_eacc = 0; // Prevent windup
  }

  // Don't build up 'negative' force below threshold
  if (intg_eacc < 0) intg_eacc = 0;

  // PID in standard form
  float pid_val =
      cnt_seti.kp * hwdesc.dac_range * inr *
          (eacc + cnt_seti.ki * intg_eacc + cnt_seti.kd * diff_eacc) +
      hwdesc.dac_min;

  if (kmh < 15.f) {
    pid_val = 0;
    intg_eacc = 0;
  }

  // this lowers the gain to around 3/4 !!!
  // and is almost linear in that range :/
  // slope between 0.75 and 0.6
  // gives a bit lower deceleration for the bigger errors !
  // but why ??
  if (pid_val > 100)
    pid_val = powf(pid_val, 0.95f); // TODO: FIXME: remove that line

  // PREVENT OVERFLOW WINDUP !!!
  if (pid_val > hwdesc.dac_max) intg_eacc -= eacc;

  if (pid_val < hwdesc.dac_min) pid_val = hwdesc.dac_min;
  if (pid_val > hwdesc.dac_max) pid_val = hwdesc.dac_max;

  current_frq = (int)roundf(pid_val + cnt_seti.ff_pid2);

  prev_eacc = acc;
}
