
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
*   net.h contains the public networking function declarations.
*
*/

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/timers.h>
#include <mdns.h>
#pragma once

void initialise_wifi(void);
void wifi_apsta();
void initialise_mdns(void);
httpd_handle_t start_webserver(void);
void wss_server_send_messages(httpd_handle_t *server);

#define EXAMPLE_ESP_WIFI_SSID "DYNO9126"
#define EXAMPLE_ESP_WIFI_PASS "DYNO9126"
#define EXAMPLE_ESP_WIFI_CHANNEL 10
#define EXAMPLE_MAX_STA_CONN 1

#define MOS 3 // MAX open sockets - 1 ws + 1 http
