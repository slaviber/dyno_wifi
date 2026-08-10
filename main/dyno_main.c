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
*   dyno_main.c contains the entry point for the Process Controller.
*   All RTOS tasks are defined here - one per core.
*
*/

#include "data.h"
#include "init.h"
#include "net.h"
#include "logic.h"
#include <esp_log.h>
#include <nvs.h>

// 4, 6, 7, 8, 9, 10, 11, 12, 13, 23, 24, 29, 30, 31, 33, 36, 37 - used pins (pink)
// 16 26 AUX pins, 5 Input Only, 14 Output Only - usable (green)
// 1 2 3 15 17 18 19 20 21 22 25 27 28 32 34 35 38 39 unusable pins (yellow)

// 6to7 - 6 is RPM1 CTRL, 7 is be RPM2 CTRL
// 12to13 - 13 is RPM PULSE, 12 is RPM clock 500 kHz
// 23to24 - 23 is TENSO PULSE, 24 is 40MHz clock
// 33toschmidt - 33 is TENSO CTRL
// 29toCS0 30toCLK 31toMISO 37toMOSI - SPI3
// 36 is DDS chip MCLK
// 4 is Status Chain IN
// 8, 9, 10 is StdIO
// 11 is DAC/PLL out
//  ^ PINK

// This task goes to core 0 w/ AUX functions
void netTask(void *pvParameters) {
  const char *taskMessage = "Networking running on core ";
  int coreid = xPortGetCoreID();
  printf("%s %d\n", taskMessage, coreid);

  ESP_LOGW(TAG, "Start APSTA Mode");

  initialise_wifi();
  wifi_apsta();

  initialise_mdns();

  static httpd_handle_t server = NULL;

  // webserver_init(server);
  server = start_webserver();
  // server = server;
  wss_server_send_messages(&server);

  vTaskDelete(NULL);
  return;
}

// This task goes to core 1 - PID LOOP
void coreTask(void *pvParameters) {
  const char *taskMessage = "Task dyno running on core ";
  int coreid = xPortGetCoreID();
  printf("%s %d\n", taskMessage, coreid);

  // MAX PRIORITY: GPIO_INTR level 3
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
  // 10 kHz timer is level3 ?!

  inputs_init();
  init_pc_consts(&consts); // Init all the constants which are not constexprs
  tg0_timer0_init();

  while (1) {
    blockingStatusChaining();
  }

  vTaskDelete(NULL);
  return;
}

void app_main(void) {

  main_init();

  go_to_failsafe(); // Reset brake

  nvs_preconfig();

  // Example of nvs_get_stats() to get the number of used entries and free
  // entries:
  nvs_stats_t nvs_stats;
  nvs_get_stats(NULL, &nvs_stats);
  printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
         nvs_stats.used_entries, nvs_stats.free_entries,
         nvs_stats.total_entries);

  cdrag(); // After the settings are read from the nvs!

  xTaskCreatePinnedToCore(coreTask,   /* Function to implement the task */
                          "coreTask", /* Name of the task */
                          10000,      /* Stack size in words */
                          NULL,       /* Task input parameter */
                          1,          /* Priority of the task, 0 is LOWEST */
                          NULL,       /* Task handle. */
                          1);         /* Core where the task should run */

  xTaskCreatePinnedToCore(netTask,   /* Networking on core 0, LOWEST priority */
                          "netTask", /* Name of the task */
                          10000,     /* Stack size in words */
                          NULL,      /* Task input parameter */
                          0,         /* Priority of the task, 0 is LOWEST */
                          NULL,      /* Task handle. */
                          0);        /* Core where the task should run */

  gpio_set_direction(LIFT_OUT_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LIFT_OUT_GPIO, LB_DN); // LB_DOWN
  gpio_set_direction(FAN_OUT_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(FAN_OUT_GPIO, 0); // FAN OFF
  gpio_set_direction(ALRM_IN_GPIO, GPIO_MODE_INPUT);
  gpio_pullup_dis(ALRM_IN_GPIO);
}
