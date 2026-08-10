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
*   init.c contains the initialization code for the Process Controller
*
*/

#include "init.h"
#include "logic.h"
#include <driver/gptimer.h>
#include <nvs.h>
#include <driver/mcpwm_prelude.h>
#include <nvs_flash.h>

#define PCNT_L_LIM_VAL SHRT_MIN // PCNT_LL_MIN_LIN // SHRT_MIN // -32768
#define PCNT_H_LIM_VAL SHRT_MAX // PCNT_LL_MAX_LIN // SHRT_MAX // 32767

#define PCNT_SIG_1 2  // Pulse Input GPIO / 40 MHz
#define PCNT_SIG_2 14 // Pulse Input GPIO / 500 kHz

#define PCNT_CTRL_1 21 // Control GPIO HIGH=count up, LOW=count down A/I 1
#define PCNT_CTRL_4 32 // Control GPIO HIGH=count up, LOW=count down S/I 1
#define PCNT_CTRL_2 34 // Control GPIO HIGH=count up, LOW=count down D/I 1
#define PCNT_CTRL_3 35 // Control GPIO HIGH=count up, LOW=count down D/I 2

void nvs_create_blob(const char *key, nvs_handle_t handle, void *default_val,
                     size_t default_len) {
  size_t len = 0;
  esp_err_t ret = nvs_get_blob(handle, key, NULL, &len);
  if (ret == ESP_ERR_NVS_NOT_FOUND) {
    ESP_ERROR_CHECK(nvs_set_blob(handle, key, default_val, default_len));
    nvs_commit(handle);
  } else if (ret != ESP_OK) ESP_ERROR_CHECK(ret);
}

void nvs_preconfig() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  nvs_handle_t h_wifi;
  ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &h_wifi));
  nvs_create_blob("ssid", h_wifi, (void *)"", 1);
  nvs_create_blob("pass", h_wifi, (void *)"", 1);
  nvs_commit(h_wifi);
  nvs_close(h_wifi);

  nvs_handle_t h_dyno;
  ESP_ERROR_CHECK(nvs_open("dyno", NVS_READWRITE, &h_dyno));
  nvs_create_blob("conf", h_dyno, (void *)&cnt_seti, sizeof(cnt_seti));
  size_t lenc = 0;
  ESP_ERROR_CHECK(nvs_get_blob(h_dyno, "conf", NULL, &lenc));
  ESP_LOGI(TAG, "cnt_seti LENGTH IS: %d and should be: %d", lenc,
           sizeof(cnt_seti));
  if (lenc != sizeof(cnt_seti)) {
    // Wrong config version :(
    // Leave it AS IS
    ESP_LOGW(TAG, "DYNO CONFIG FORMAT MISMATCH, CONFIG NOT LOADED! BEWARE!");
  } else
    ESP_ERROR_CHECK(nvs_get_blob(h_dyno, "conf", (void *)&cnt_seti, &lenc));
  nvs_commit(h_dyno);
  nvs_close(h_dyno);
}

static ledc_timer_config_t ledc_timer_0 = {
    .duty_resolution = LEDC_TIMER_1_BIT,
    .freq_hz = 40000000,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
};
static ledc_channel_config_t ledc_channel_0 = {.channel = LEDC_CHANNEL_0,
                                               .duty = 1,
                                               .gpio_num = 15,
                                               .speed_mode =
                                                   LEDC_HIGH_SPEED_MODE,
                                               .timer_sel = LEDC_TIMER_0};
static ledc_timer_config_t ledc_timer_1 = {
    .duty_resolution = LEDC_TIMER_1_BIT,
    .freq_hz = 500000,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = LEDC_TIMER_1,
};
static ledc_channel_config_t ledc_channel_1 = {.channel = LEDC_CHANNEL_1,
                                               .duty = 1,
                                               .gpio_num = 27, // GPIO pin
                                               .speed_mode =
                                                   LEDC_HIGH_SPEED_MODE,
                                               .timer_sel = LEDC_TIMER_1};

void set_ADC_clock_on_GPIO15(void) {
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_0)); // Set up GPIO PIN
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_0));
}
void set_Pulse_clock_on_GPIO27(void) {
  ledc_timer_config(&ledc_timer_1);
  ledc_channel_config(&ledc_channel_1);
}

#define PCNT_CTRL_PIN_TO_NUMBER(n)                                             \
  case PCNT_CTRL_##n:                                                          \
    return n - 1;                                                              \
    break;

static uint8_t input_number(gpio_num_t input) { // FIXME: This is hard-coded!
  switch (input) {
    FOREACH(PCNT_CTRL_PIN_TO_NUMBER, 1, 2, 3, 4) // 4 inputs
  default:
    ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
    return -1;
  }
}

static void pcnt_Pulse_init(gpio_num_t PCNT_Pulse_SIG,
                            gpio_num_t PCNT_Pulse_CTRL, const char tag) {
  pcnt_unit_handle_t pcnt_pulse_unit = NULL;
  pcnt_unit_handle_t pcnt_pulse_unit_c = NULL; // Complementary unit

  pcnt_unit_config_t unit_config = {
      .high_limit = PCNT_H_LIM_VAL,
      .low_limit = PCNT_L_LIM_VAL,
  };
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_pulse_unit));
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_pulse_unit_c));

  pcnt_chan_config_t chan_config = {
      .edge_gpio_num = PCNT_Pulse_SIG,
      .level_gpio_num = PCNT_Pulse_CTRL,
  };
  pcnt_channel_handle_t pcnt_chan = NULL;
  ESP_ERROR_CHECK(pcnt_new_channel(pcnt_pulse_unit, &chan_config, &pcnt_chan));
  pcnt_channel_handle_t pcnt_chan_c = NULL;
  ESP_ERROR_CHECK(
      pcnt_new_channel(pcnt_pulse_unit_c, &chan_config, &pcnt_chan_c));

  // Count up on the positive AND on the negative edge
  ESP_ERROR_CHECK(
      pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                   PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  // Stop counting when low, keep counting wen high
  ESP_ERROR_CHECK(
      pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                    PCNT_CHANNEL_LEVEL_ACTION_HOLD));
  // Count up on the positive AND on the negative edge
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      pcnt_chan_c, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  // Stop counting when high, keep counting wen low !!!!!! Complementary !!!
  ESP_ERROR_CHECK(
      pcnt_channel_set_level_action(pcnt_chan_c, PCNT_CHANNEL_LEVEL_ACTION_HOLD,
                                    PCNT_CHANNEL_LEVEL_ACTION_KEEP));

  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_pulse_unit, PCNT_H_LIM_VAL));
  ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_pulse_unit_c, PCNT_H_LIM_VAL));

  pcnt_event_callbacks_t cbs = {
      .on_reach = pulse_ouflow_handler,
  };
  pcnt_isr_ctx *ctx = malloc(sizeof(pcnt_isr_ctx)); // FIXME: Unmanaged memory
  ctx->unit = pcnt_pulse_unit;
  ctx->unit_c = pcnt_pulse_unit_c;
  ctx->control = PCNT_Pulse_CTRL;
  ctx->input_nr = input_number(ctx->control);
  ctx->value = &rpm[ctx->input_nr];

  ESP_ERROR_CHECK(
      pcnt_unit_register_event_callbacks(pcnt_pulse_unit, &cbs, ctx));
  ESP_ERROR_CHECK(
      pcnt_unit_register_event_callbacks(pcnt_pulse_unit_c, &cbs, ctx));

  pcnt_unit_clear_count(pcnt_pulse_unit);
  pcnt_unit_enable(pcnt_pulse_unit);
  pcnt_unit_start(pcnt_pulse_unit);
  pcnt_unit_clear_count(pcnt_pulse_unit_c);
  pcnt_unit_enable(pcnt_pulse_unit_c);
  pcnt_unit_start(pcnt_pulse_unit_c);

  gpio_set_intr_type(PCNT_Pulse_CTRL, GPIO_INTR_LOW_LEVEL);
  gpio_isr_handler_add(PCNT_Pulse_CTRL, (void (*)(void *))Pulse_isr_handler,
                       (void *)ctx);
  gpio_intr_enable(PCNT_Pulse_CTRL);

  input_arr[ctx->input_nr] = INPUT_PULS;
  puls_arr[puls_idx++] = ctx->input_nr;
  if (puls_idx > N_INPUTS) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);

  if (tag >= '!' && tag <= '~') {
    input_tag[(int)tag] = ctx->input_nr;
    tag_arr[ctx->input_nr] = tag;
  }
}

static void pcnt_ADC_init(gpio_num_t PCNT_ADC_SIG, gpio_num_t PCNT_ADC_CTRL,
                          const char tag) {
  pcnt_unit_handle_t pcnt_ADC_unit = NULL;

  pcnt_unit_config_t unit_config = {
      .high_limit = PCNT_H_LIM_VAL,
      .low_limit = PCNT_L_LIM_VAL,
  };
  ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_ADC_unit));

  pcnt_chan_config_t chan_config = {
      .edge_gpio_num = PCNT_ADC_SIG,
      .level_gpio_num = PCNT_ADC_CTRL,
  };
  pcnt_channel_handle_t pcnt_ADC_chan = NULL;
  ESP_ERROR_CHECK(
      pcnt_new_channel(pcnt_ADC_unit, &chan_config, &pcnt_ADC_chan));

  // Count up on the positive AND on the negative edge
  ESP_ERROR_CHECK(pcnt_channel_set_edge_action(
      pcnt_ADC_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
  // Stop counting when low, keep counting wen high
  ESP_ERROR_CHECK(pcnt_channel_set_level_action(
      pcnt_ADC_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
      PCNT_CHANNEL_LEVEL_ACTION_HOLD));

  pcnt_unit_clear_count(pcnt_ADC_unit);
  pcnt_unit_enable(pcnt_ADC_unit);
  pcnt_unit_start(pcnt_ADC_unit);

  gpio_set_intr_type(PCNT_ADC_CTRL, GPIO_INTR_LOW_LEVEL);
  pcnt_isr_ctx *ctx = malloc(sizeof(pcnt_isr_ctx)); // FIXME: Unmanaged memory
  ctx->unit = pcnt_ADC_unit;
  ctx->unit_c = NULL;
  ctx->control = PCNT_ADC_CTRL;
  ctx->input_nr = input_number(ctx->control);
  ctx->value = &captured[ctx->input_nr];
  gpio_isr_handler_add(PCNT_ADC_CTRL, (void (*)(void *))ADC_isr_handler,
                       (void *)ctx);
  gpio_intr_enable(PCNT_ADC_CTRL);

  input_arr[ctx->input_nr] = INPUT_ADC;
  adc_arr[adc_idx++] = ctx->input_nr;
  if (adc_idx > N_INPUTS) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);

  if (tag >= '!' && tag <= '~') {
    input_tag[(int)tag] = ctx->input_nr;
    tag_arr[ctx->input_nr] = tag;
  }
}

void tg0_timer0_init() {
  gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = (int)(hwdesc.pc_clock) * 10, // 100 kHz timer
      .flags.intr_shared = 0,
  };
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_alarm_cb,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
  ESP_LOGI(TAG, "Enable timer");
  ESP_ERROR_CHECK(gptimer_enable(gptimer));
  ESP_LOGI(TAG, "Start timer, stop it at alarm event");
  gptimer_alarm_config_t alarm_config1 = {
      .alarm_count = 10, // 10 kHz
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };

  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));
  ESP_ERROR_CHECK(gptimer_start(gptimer));
}

void DDS_init() {
  mcpwm_timer_config_t timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_PLL160M,
      .resolution_hz = 80000000,
      .period_ticks = 2048,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
      .flags.update_period_on_empty = 1, // PWM_TIMER0_PERIOD_UPMETHOD
  };
  mcpwm_new_timer(&timer_config, &adc_timer);

  mcpwm_oper_handle_t oper = NULL;
  mcpwm_operator_config_t operator_config = {
      .group_id = 0,
  };
  mcpwm_new_operator(&operator_config, &oper);
  mcpwm_operator_connect_timer(oper, adc_timer);
  mcpwm_gen_handle_t generator = NULL;
  mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = GPIO_NUM_26,
  };
  mcpwm_new_generator(oper, &generator_config, &generator);
  mcpwm_generator_set_action_on_timer_event(
      generator, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                              MCPWM_TIMER_EVENT_FULL,
                                              MCPWM_GEN_ACTION_TOGGLE));
  mcpwm_timer_enable(adc_timer);
  mcpwm_timer_start_stop(adc_timer, MCPWM_TIMER_START_NO_STOP);
}

// Initialize the inputs
void inputs_init() {
  // # is CTRL input nr; * is _idx_nr; doubly associative array:

  // 1x TENSO
  pcnt_ADC_init(PCNT_SIG_1, PCNT_CTRL_1, 'b'); // Brake input
  // &captured[#0] -> adc_arr[*0]=#0, adc_idx=*(++0), input_arr[#0]=INPUT_ADC, input_tag['b'] = #0, tag_arr[#0] = 'b'

  // 2x RPM
  pcnt_Pulse_init(PCNT_SIG_2, PCNT_CTRL_2, 'r'); // brake Roller
  // &rpm[#1] -> puls_arr[*0]=#1, puls_idx=*(++0), input_arr[#1]=INPUT_PULS, input_tag['r'] = #1. tag_arr[#1] = 'r'

  pcnt_Pulse_init(PCNT_SIG_2, PCNT_CTRL_3, 'i'); // Idler roller
  // &rpm[#2] -> puls_arr[*1]=#2, puls_idx=*(++1), input_arr[#2]=INPUT_PULS, input_tag['i'] = #2, tag_arr[#2] = 'i'
}

void init_pc_consts(pc_consts *c) {
  // Calculate the PID constants
  c->pid1_min_pct =
      calc_percent(hwdesc.adc_min, hwdesc.adc_range_inv, hwdesc.pid1_min);
  c->pid1_max_pct =
      calc_percent(hwdesc.adc_min, hwdesc.adc_range_inv, hwdesc.pid1_max);

  // Choose the process variable sources
  int idx = -1;

  idx = input_tag['b']; // This is the brake ADC input
  if (idx < 0) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
  c->adc_force = idx;

  idx = input_tag['r']; // This is the brake roller Pulse input
  if (idx < 0) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
  // Keep the loop closed !!!!
  // The pid input is on the brake roller !!!
  c->kmh_brake = idx;

  idx = input_tag['i']; // This is the idle roller Pulse input
  if (idx < 0) ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
  c->kmh_idler = idx;
}

void main_init(void) {
  atomic_init(&dither_counter, 0);
  // atomic_init(&dds_freq, 10737418 - 50000);
  atomic_init(&current_frq, hwdesc.dac_unload);
  atomic_init(&disp_frq_prg[0], hwdesc.dac_unload);
  atomic_init(&disp_frq_prg[1], 0);
  atomic_init(&adc_idx, 0);
  atomic_init(&puls_idx, 0);

  // atomic_init(&kmh1_pv, NULL);
  // atomic_init(&kmh2_pv, NULL);
  // atomic_init(&n101_pv, NULL);

  atomic_init(&current_prg, 0);
  atomic_init(&drag_coef, 0.f);
  atomic_init(&whr_inv, 0.f);

  for (int N = 0; N < N_INPUTS; ++N) {
    atomic_init(&rpm[N], RPM_MAX);
    atomic_init(&no_rpm[N], 0);
    atomic_init(&captured[N], 0);
    atomic_init(&filt_in[N], 0);
    atomic_init(&graph_val[N], 0);
    atomic_init(&input_arr[N], INPUT_NONE);
    atomic_init(&adc_arr[N], -1);
    atomic_init(&puls_arr[N], -1);
    atomic_init(&tag_arr[N], ' ');
  }

  for (int N = 0; N < 256; N++) {
    atomic_init(&input_tag[N], -1);
  }

  atomic_init(&mop_change, true);
  atomic_init(&n10_change, true);
  atomic_init(&kmh_change, true);

  for (int N = 0; N < STATE_END; ++N) {
    atomic_init(&pc_state[N], false);
  }

  for (int N = 0; N < sizeof(dyn_state) / sizeof(typeof(dyn_state[0])); ++N) {
    atomic_init(&dyn_state[N], 0);
  }

  // iir_transposed_form_II_initialization(&besself[0]);
  // iir_transposed_form_II_initialization(&besself[1]);

  ESP_ERROR_CHECK(uart_driver_delete(0));
  ESP_ERROR_CHECK(uart_driver_install(
      0, 256, 16384, 0, NULL,
      ESP_INTR_FLAG_IRAM |
          ESP_INTR_FLAG_LEVEL1)); // 10k timer is level3, gpio is level2
  ESP_ERROR_CHECK(uart_set_baudrate(0, 921600));

  ESP_LOGI(TAG, "UART _ RECONFIGURED");
  //*** */
  uart_config_t uart_config = {
      .baud_rate = 921600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
  // Set UART pins(TX: IO4, RX: IO5, RTS: IO18, CTS: IO19)
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, 36,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(
      UART_NUM_1, 1024, 256, 0, NULL,
      ESP_INTR_FLAG_IRAM |
          ESP_INTR_FLAG_LEVEL1)); // uart 1 for status chaining (tm)
  // ESP_ERROR_CHECK(uart_set_baudrate(1, 921600));
  ESP_LOGI(TAG, "UART1 _ CONFIGURED");
  //*** */
  // spi_init();
  DDS_init();
  // configureClockOut(); // 5 MHz DDS clock out

  // REF OUTS
  set_ADC_clock_on_GPIO15();
  set_Pulse_clock_on_GPIO27();
}
