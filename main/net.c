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
*   net.c is the networking implementation.
*
*/

#include "net.h"
#include "data.h"
#include "logic.h"
#include "cJSON.h"
#include <nvs.h>
#include <math.h>

// The wifi reconnect timer
static TimerHandle_t wifi_t;
static cJSON *pc_timestring();


// static EventGroupHandle_t wifi_event_group;
// const int CONNECTED_BIT = BIT0;
void wifi_reconnect(TimerHandle_t xTimer) { esp_wifi_connect(); }

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    // esp_wifi_set_mode(WIFI_MODE_AP);
    xTimerStart(wifi_t, 0);
    // xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
    // xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
  }
}

void initialise_wifi(void) {
  esp_log_level_set("wifi", ESP_LOG_WARN);

  ESP_ERROR_CHECK(esp_netif_init());
  // wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  assert(ap_netif);
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_t = xTimerCreate("WifiReconnect", 10000 / portTICK_PERIOD_MS, pdFALSE,
                        (void *)1, &wifi_reconnect);
}

void wifi_apsta() {
  wifi_config_t ap_config = {0};
  strcpy((char *)ap_config.ap.ssid, EXAMPLE_ESP_WIFI_SSID);
  strcpy((char *)ap_config.ap.password, EXAMPLE_ESP_WIFI_PASS);
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap_config.ap.ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID);
  ap_config.ap.max_connection = EXAMPLE_MAX_STA_CONN;
  ap_config.ap.channel = EXAMPLE_ESP_WIFI_CHANNEL;

  nvs_handle_t h_wifi;
  ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &h_wifi));

  size_t lenp = 0;
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "pass", NULL, &lenp));
  if (lenp > 32) lenp = 32;
  char pass[lenp + 1];
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "pass", pass, &lenp));

  size_t lens = 0;
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "ssid", NULL, &lens));
  if (lens > 32) lens = 32;
  char ssid[lens + 1];
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "ssid", ssid, &lens));

  nvs_close(h_wifi);

  pass[lenp - 0] = 0; // Force null-terminated
  ssid[lens - 0] = 0;

  wifi_config_t sta_config = {0};
  strcpy((char *)sta_config.sta.ssid, ssid);
  strcpy((char *)sta_config.sta.password, pass);
  sta_config.sta.scan_method = WIFI_FAST_SCAN;
  sta_config.sta.failure_retry_cnt = 3;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_ps(WIFI_PS_NONE);
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s channel:%d",
           EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS,
           EXAMPLE_ESP_WIFI_CHANNEL);

  esp_err_t e = esp_wifi_connect();
  ESP_ERROR_CHECK_WITHOUT_ABORT(e);
  ESP_LOGI(TAG, "WIFI_MODE_STA SSID:%s password:%s", ssid, pass);
  if ((ESP_OK != e) && (ESP_ERR_WIFI_SSID != e)) ESP_ERROR_CHECK(e);
  if (ESP_OK == e)
    ESP_LOGI(TAG, "WIFI_MODE_STA connected. SSID:%s password:%s", ssid, pass);
}

void initialise_mdns(void) {
  mdns_init();
  mdns_hostname_set(TAG);
  mdns_instance_name_set(TAG);

  mdns_txt_item_t serviceTxtData[] = {{"board", "esp32"}, {"path", "/"}};

  ESP_ERROR_CHECK(
      mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                       sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

void set_cors_headers(httpd_req_t *req) {
  // httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  // httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "*");
  // httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "*");
}

void http_send_response(httpd_req_t *req, const char *code, const char *payload,
                        int len) {
  if (len < 1 || len > 2048) return; // Too bad
  // set_cors_headers(req);
  httpd_resp_set_status(req, code);
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, payload, len);
}

void http_send_status(httpd_req_t *req, const char *code) {
  http_send_response(req, code, "", 1);
}

static cJSON *pc_timestring() {
  char buf[21];
  int len = snprintf(buf, 21, "%" PRIu64, pc_clock());
  if (len <= 0 || len > 21) {
    ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE); // This is pretty concerning :/
  }
  return cJSON_CreateString(buf);
}

void http_send_200plustime(httpd_req_t *req) {
  cJSON *rdata = cJSON_CreateObject();

  cJSON_AddItemToObject(rdata, "time", pc_timestring());
  char *result = cJSON_PrintUnformatted(rdata);
  int reslen = strnlen(result, 32);
  http_send_response(req, "200 OK", result, reslen);
  cJSON_Delete(rdata);
}

static esp_err_t hwinfo_get_handler(httpd_req_t *req) {
  cJSON *data = cJSON_CreateObject();
  JSN_STRUCT(HWINFO, hwdesc, data);

  cJSON *hwdesc = cJSON_CreateObject();
  cJSON_AddItemToObject(hwdesc, "hwdesc", data);
  char *result = cJSON_PrintUnformatted(hwdesc);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  set_cors_headers(req);
  httpd_resp_send(req, result, strlen(result) + 1);

  free(result);
  cJSON_Delete(hwdesc);
  return ESP_OK;
}

// Immediately halt the controller
static esp_err_t halt_put_handler(httpd_req_t *req) {
  go_to_failsafe();

  set_cors_headers(req);
  http_send_status(req, "200 OK");
  return ESP_OK;
}

// Upto 200 floats, format %3.2f
// JSON: {mop: MOP, len: LEN, data: [xxx.xx, xxx.xx, xxx.xx, ..., xxx.xx]}
// max 2048 b
static esp_err_t dyno_swp_post_handler(httpd_req_t *req) {
  set_cors_headers(req);
  size_t len = req->content_len;
  if (!len) {
    http_send_response(req, "400 Bad Request", "len=0", 6);
    go_to_failsafe();
    return ESP_FAIL;
  }
  if (len > 2048) { // Spare the RAM
    http_send_status(req, "413 Content Too Large");
    go_to_failsafe();
    return ESP_FAIL;
  }

  // Accept data only when this MOP is set!
  if (MOD_SWEEP != cnt_seti.mop) {
    http_send_status(req, "409 Conflict");
    go_to_failsafe();
    return ESP_FAIL;
  }

  int ret;
  char *buf = (char *)malloc(len);
  int clen = 0;
  float data0 = 0.f;
  bool state0 = false;
  // Get the whole payload
  while ((clen < len) &&
         (ret = httpd_req_recv(req, &buf[clen], len - clen)) > 0)
    clen += ret;
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    } else http_send_response(req, "400 Bad Request", "req_recv error", 15);
    go_to_failsafe();
    return ESP_FAIL;
  }

  cJSON *root = cJSON_ParseWithLength(buf, len);
  enum RESPONSE resp = REST_OK;
  // enum RESPONSE resp_sws = REST_OK;
  enum MODE_OF_OPERATION mop = MOP_END;
  int data_len = 0;

  if (!cJSON_IsObject(root)) resp = REST_BAD;

  cJSON *res = cJSON_GetObjectItem(root, "mop");
  if (REST_OK == resp && res) {
    if (!cJSON_IsNumber(res)) resp = REST_BAD;
    mop = res->valueint;
    if (!(mop >= 0 && mop < hwdesc.nr_mops)) resp = REST_BAD;
  } else resp = REST_BAD;

  res = cJSON_GetObjectItem(root, "len");
  if (REST_OK == resp && res) {
    if (!cJSON_IsNumber(res)) resp = REST_BAD;
    data_len = res->valueint;
    if (!(data_len > 0 && data_len <= hwdesc.buflen)) resp = REST_BAD;
  } else resp = REST_BAD;

  res = cJSON_GetObjectItem(root, "data");
  if (REST_OK == resp && res) {
    if (!cJSON_IsArray(res)) resp = REST_BAD;
    else {
      int array_len = cJSON_GetArraySize(res);
      if (array_len != data_len) resp = REST_BAD;
      else {
        float *data = (float *)malloc(array_len * sizeof(float));
        int i = -1;
        cJSON *sub;
        cJSON_ArrayForEach(sub, res) {
          i++;
          if (!cJSON_IsNumber(sub)) {
            resp = REST_BAD;
            break;
          } else {
            data[i] = sub->valuefloat;
            if (data[i] < 0.f || data[i] > 1e6f) {
              resp = REST_BAD;
              break;
            }
          }
        }
        if (REST_OK == resp) {
          sws.sweep_mode = mop;
          data0 = data[0];
          state0 = sws.sweep_run;
          resp = sweep_top(data, array_len);
        }
        free(data);
      }
    }
  } else resp = REST_BAD;

  cJSON_Delete(root);
  free(buf);

  if (REST_OK == resp && false == state0 && !isnanf(data0)) {
    // Bootstrap the controller
    switch (sws.sweep_mode) {
    case CONST_KMH:
      cnt_seti.target_kmh = data0;
      break;
    case CONST_N10:
      cnt_seti.target_n10 = data0;
      break;
    case CONST_FRQ:
      cnt_seti.target_frq = (int)roundf(data0);
      break;
    default:
      resp = REST_BAD;
      break;
    }
    // Status update
    pc_state[SWP_STATE] = sws.sweep_run;
    current_prg = sws.remaining_size[0] + sws.remaining_size[1];
  }

  if (resp == REST_OK) {
    // Cannot send an 1xx response for some reason :(((
    // http_send_status(req, "100 Continue");
    http_send_status(req, "200 OK");
    return ESP_OK;
  } else if (REST_AGAIN == resp) {
    http_send_status(req, "507 Insufficient Storage");
    return ESP_OK;
  } else {
    http_send_response(req, "400 Bad Request", "ESP_FAIL", 9);
    go_to_failsafe();
    return ESP_FAIL;
  }
  return ESP_OK;
}

/* An HTTP GET handler */
static esp_err_t wifi_ssid_get_handler(httpd_req_t *req) {
  nvs_handle_t h_wifi;
  ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &h_wifi));
  size_t len = 0;
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "ssid", NULL, &len));
  if (len > 32) len = 32;
  char ssid[len];
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "ssid", ssid, &len));
  nvs_close(h_wifi);

  //ESP_LOGI(TAG, "WIFI SSID IS:%s", ssid);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  set_cors_headers(req);
  httpd_resp_send(req, ssid, len);
  return ESP_OK;
}

/* An HTTP GET handler */
static esp_err_t wifi_pass_get_handler(httpd_req_t *req) {
  nvs_handle_t h_wifi;
  ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &h_wifi));
  size_t len = 0;
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "pass", NULL, &len));
  if (len > 32) len = 32;
  char pass[len];
  ESP_ERROR_CHECK(nvs_get_blob(h_wifi, "pass", pass, &len));
  nvs_close(h_wifi);

  //ESP_LOGI(TAG, "WIFI PASS IS:%s", pass);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  set_cors_headers(req);
  httpd_resp_send(req, pass, len);
  return ESP_OK;
}

/* An HTTP PUT handler. */
static esp_err_t wifi_cred_put_handler(httpd_req_t *req) {
  size_t len = req->content_len;
  set_cors_headers(req);
  if (len > 128) { // Spare the RAM
    len = 128;
    http_send_status(req, "414 URI Too Long");
    return ESP_FAIL;
  }

  char buf[len];
  int ret;
  int ssid_len;
  int pass_len;
  char *ssid;
  char *pass;

  //TODO: FIXME: There is no guarantee that buflen wil == len!
  // Look the sweep handler!!!
  if ((ret = httpd_req_recv(req, buf, len)) <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  cJSON *root = cJSON_ParseWithLength((char *)buf, len);
  enum RESPONSE resp = REST_OK;
  cJSON *res = cJSON_GetObjectItem(root, "wifi");

  if (res && cJSON_IsObject(res)) {
    cJSON *sub;

    sub = cJSON_GetObjectItem(res, "ssid_len");
    if (REST_OK == resp && sub && cJSON_IsNumber(sub)) {
      int len = sub->valueint;
      if (len > 0 && len < 32) ssid_len = len;
      else resp = REST_BAD;
    } else resp = REST_BAD;

    sub = cJSON_GetObjectItem(res, "pass_len");
    if (REST_OK == resp && sub && cJSON_IsNumber(sub)) {
      int len = sub->valueint;
      if (len > 0 && len < 32) pass_len = len;
      else resp = REST_BAD;
    } else resp = REST_BAD;

    sub = cJSON_GetObjectItem(res, "ssid");
    if (REST_OK == resp && sub && cJSON_IsString(sub)) {
      ssid = sub->valuestring;
      ssid[strnlen(ssid, 32)] = 0; // safety
    } else resp = REST_BAD;

    sub = cJSON_GetObjectItem(res, "pass");
    if (REST_OK == resp && sub && cJSON_IsString(sub)) {
      pass = sub->valuestring;
      pass[strnlen(pass, 32)] = 0; // safety
    } else resp = REST_BAD;

  } else resp = REST_BAD;

  if (resp == REST_BAD) {
    http_send_status(req, "400 Bad Request");
    return ESP_FAIL;
  } else {
    nvs_handle_t h_wifi;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &h_wifi));
    ESP_ERROR_CHECK(nvs_set_blob(h_wifi, "ssid", ssid, ssid_len));
    nvs_commit(h_wifi);
    ESP_ERROR_CHECK(nvs_set_blob(h_wifi, "pass", pass, pass_len));
    nvs_commit(h_wifi);
    nvs_close(h_wifi);

    ESP_LOGI(TAG, "WIFI SSID SET:%s", ssid);
    ESP_LOGI(TAG, "WIFI PASS SET:%s", pass);

    pc_state[PARAM_CHG] = true; // Signal change
    // http_send_status(req, "200 OK");
    http_send_200plustime(req);
    return ESP_OK;
  }
}

/* An HTTP GET handler */
static esp_err_t dyno_seti_get_handler(httpd_req_t *req) {
  cJSON *data = cJSON_CreateObject();
  JSN_STRUCT(SETTINGS_T, cnt_seti, data);

  cJSON *seti = cJSON_CreateObject();
  cJSON_AddItemToObject(seti, "seti", data);
  char *result = cJSON_PrintUnformatted(seti);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  set_cors_headers(req);
  httpd_resp_send(req, result, strlen(result) + 1);

  free(result);
  cJSON_Delete(seti);

  return ESP_OK;
}

/* An HTTP PUT handler. Update mode settings without saving. Only parts of the
 * config can be updated to save traffic! */
static esp_err_t dyno_seti_put_handler(httpd_req_t *req) {
  size_t len = req->content_len;
  if (len > 512) { // Spare the RAM
    len = 512;
    httpd_resp_set_status(req, "414 URI Too Long");
    httpd_resp_set_type(req, "text/plain");
    set_cors_headers(req);
    httpd_resp_send(req, "", 1);
    return ESP_FAIL;
  }

  char buf[len];
  int ret;

  if ((ret = httpd_req_recv(req, buf, len)) <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      set_cors_headers(req);
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  cJSON *root = cJSON_ParseWithLength((char *)buf, len);
  enum RESPONSE resp = REST_BAD;

  cJSON *res = cJSON_GetObjectItem(root, "seti");
  if (res) {
    resp = REST_OK;
    cJSON *sub;
    JSN_PARSE(SETTINGS_T, sub, res, cnt_seti, resp);
  }

  cJSON_Delete(root);

  if (resp == REST_OK) {

    pc_state[PARAM_CHG] = true; // Signal change
    http_send_200plustime(req);
    return ESP_OK;
  } else {

    nvs_handle_t h_dyno;
    ESP_ERROR_CHECK(nvs_open("dyno", NVS_READONLY, &h_dyno));
    size_t lenc = 0;
    ESP_ERROR_CHECK(nvs_get_blob(h_dyno, "conf", NULL, &lenc));
    if (lenc != sizeof(cnt_seti))
      ESP_LOGW(TAG, "DYNO CONFIG FORMAT MISMATCH, CONFIG NOT LOADED! BEWARE!");
    else
      ESP_ERROR_CHECK(nvs_get_blob(h_dyno, "conf", (void *)&cnt_seti, &lenc));
    nvs_close(h_dyno);

    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    set_cors_headers(req);
    httpd_resp_send(req, "", 1);
    return ESP_FAIL;
  }
  return ESP_OK;
}

/* An HTTP PUT handler. Save current settings */
static esp_err_t dyno_save_put_handler(httpd_req_t *req) {
  nvs_handle_t h_dyno;
  ESP_ERROR_CHECK(nvs_open("dyno", NVS_READWRITE, &h_dyno));
  ESP_ERROR_CHECK(
      nvs_set_blob(h_dyno, "conf", (void *)&cnt_seti, sizeof(cnt_seti)));
  nvs_commit(h_dyno);
  nvs_close(h_dyno);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/plain");
  set_cors_headers(req);
  httpd_resp_send(req, "", 1);
  return ESP_OK;
}

static esp_err_t dyno_data_ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    // sws_reset(); // The sender has already disconnected, buffer state is scrambled
    // HINT: Let The UI consider if the sweep has failed anyway
    cdrag();
    return ESP_OK;
  }
  // Don't listen!
  return ESP_OK;
}

static const httpd_uri_t hwinfo_get = {.uri = "/hwinfo",
                                       .method = HTTP_GET,
                                       .handler = hwinfo_get_handler,
                                       .user_ctx = NULL};

static const httpd_uri_t halt_put = {.uri = "/HALT",
                                     .method = HTTP_PUT,
                                     .handler = halt_put_handler,
                                     .user_ctx = NULL};

static const httpd_uri_t dyno_swp_post = {.uri = "/sweep_data",
                                          .method = HTTP_POST,
                                          .handler = dyno_swp_post_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t wifi_ssid_get = {.uri = "/wifi_ssid",
                                          .method = HTTP_GET,
                                          .handler = wifi_ssid_get_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t wifi_pass_get = {.uri = "/wifi_pass",
                                          .method = HTTP_GET,
                                          .handler = wifi_pass_get_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t wifi_cred_put = {.uri = "/wifi_cred",
                                          .method = HTTP_PUT,
                                          .handler = wifi_cred_put_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t dyno_seti_get = {.uri = "/dyno_seti",
                                          .method = HTTP_GET,
                                          .handler = dyno_seti_get_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t dyno_seti_put = {.uri = "/dyno_seti",
                                          .method = HTTP_PUT,
                                          .handler = dyno_seti_put_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t dyno_save_put = {.uri = "/dyno_save",
                                          .method = HTTP_PUT,
                                          .handler = dyno_save_put_handler,
                                          .user_ctx = NULL};

static const httpd_uri_t dyno_data_ws = {.uri = "/dyno_data",
                                         .method = HTTP_GET,
                                         .handler = dyno_data_ws_handler,
                                         .user_ctx = NULL,
                                         .is_websocket = true};

static void send_data(void *arg) {
  static int wait_count = 0; // send data only 2 times a second, Wi-Fi is hell
  static char *message = NULL;
  static int len = 0;

  if (wait_count < ACCUM_WS) {
    cJSON *data = cJSON_CreateObject();
    int df = disp_frq_prg[0]; // atomic read
    cJSON_AddItemToObject(data, "freq", cJSON_CreateNumber(df));
    int di = disp_frq_prg[1]; // atomic read
    cJSON_AddItemToObject(data, "prog", cJSON_CreateNumber(di));
    int dv[N_INPUTS];
    // Atomic read. Not using memcpy to prevent possible blocking
    for (int i = 0; i < N_INPUTS; ++i) dv[i] = graph_filter.outputs[i];
    // All possible inputs
    cJSON_AddItemToObject(data, "capt", cJSON_CreateIntArray(dv, N_INPUTS));
    int ia[N_INPUTS];
    // Atomic read. Not using memcpy to prevent possible blocking
    for (int i = 0; i < N_INPUTS; ++i) ia[i] = input_arr[i];
    // + Input types
    cJSON_AddItemToObject(data, "type", cJSON_CreateIntArray(ia, N_INPUTS));
    cJSON_bool ps[STATE_END];
    // Atomic read. Not using memcpy to prevent possible blocking
    for (int i = 0; i < STATE_END; ++i) ps[i] = pc_state[i];
    /*if(pc_state[PARAM_CHG])*/ pc_state[PARAM_CHG] = false; // Signal only once
    // controller state
    // cJSON_AddItemToObject(data, "stat", cJSON_CreateBoolArray(ps, STATE_END)); TODO: FIXME: This is wasteful :(
    cJSON_AddItemToObject(data, "stat", cJSON_CreateIntArray(ps, STATE_END));
    cJSON *dyns = cJSON_CreateObject();
    // Atomic read.
    float dyn_mop = dyn_state[0];
    float dyn_frq = dyn_state[1];
    float dyn_n10 = dyn_state[2];
    float dyn_kmh = dyn_state[3];
    float dyn_acc = dyn_state[4];
    cJSON_AddItemToObject(dyns, "mop", cJSON_CreateNumber(dyn_mop));
    cJSON_AddItemToObject(dyns, "frq", cJSON_CreateNumber(dyn_frq));
    cJSON_AddItemToObject(dyns, "n10", cJSON_CreateNumber(dyn_n10));
    cJSON_AddItemToObject(dyns, "kmh", cJSON_CreateNumber(dyn_kmh));
    cJSON_AddItemToObject(dyns, "acc", cJSON_CreateNumber(dyn_acc));
    // Controller dynamic setpoints
    cJSON_AddItemToObject(data, "dyns", dyns);
    char it[N_INPUTS + 1];
    // Atomic read. Not using memcpy to prevent possible blocking
    for (int i = 0; i < N_INPUTS; ++i) it[i] = tag_arr[i];
    it[N_INPUTS] = '\0';
    // Controller input tags
    // TODO: FIXME: This is not memory-safe! cJSON uses strlen!
    cJSON_AddItemToObject(data, "tags", cJSON_CreateString(it));
    // Controller time
    cJSON_AddItemToObject(data, "time", pc_timestring());

    // TODO: FIXME: Max message size fixed to 512b due to memory concerns
    static char result[512];
    static int maxlen = 512;
    if (!cJSON_PrintPreallocated(data, result, maxlen, false)) {
      ESP_ERROR_CHECK(ESP_ERR_INVALID_SIZE);
    }
    // char *result = cJSON_PrintUnformatted(data);

    int reslen = strnlen(result, maxlen);
    char *concat = realloc(message, reslen + len + 1);
    if (!concat) {
      // free(result);
      ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    } else {
      message = concat;
      memcpy(&message[len], result, reslen + 1);
      len += reslen;
      // free(result);
    }

    cJSON_Delete(data);
    wait_count++;
  }

  if (wait_count == ACCUM_WS) {
    wait_count = 0;

    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);

    free(message);
    message = NULL;
    len = 0;
  }
}

// Get all clients and send async message
void wss_server_send_messages(httpd_handle_t *server) {
  bool send_messages = true;

  // Send async message to all connected clients that use websocket protocol
  // Send the first one immediately
  uint64_t time_next = pc_clock() /*+ SEND_INTERVAL_MS*/;
  while (send_messages) {
    while (time_next > pc_clock()) {
      taskYIELD();
    }
    // ~ 20 times a second: generate messages right at the graph frequency
    time_next /*= pc_clock()*/ += send_interval; // 12e6 === 1/20 sec

    update_bridge_state(); // lift or lower the bridge

    if (!*server) { // httpd might not have been created by now
      continue;
    }
    size_t clients = MOS;
    int client_fds[MOS];
    if (httpd_get_client_list(*server, &clients, client_fds) == ESP_OK) {
      for (size_t i = 0; i < clients; ++i) {
        int sock = client_fds[i];
        if (httpd_ws_get_fd_info(*server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
          // ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message",
          // sock);
          struct async_resp_arg *resp_arg =
              malloc(sizeof(struct async_resp_arg));
          assert(resp_arg != NULL);
          resp_arg->hd = *server;
          resp_arg->fd = sock;
          if (httpd_queue_work(resp_arg->hd, send_data, resp_arg) != ESP_OK) {
            ESP_LOGE(TAG, "httpd_queue_work failed!");
            send_messages = false;
            break;
          }
        }
      }
    } else {
      ESP_LOGE(TAG, "httpd_get_client_list failed!");
      return;
    }
  }
}

httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = 0; // Networking only on core 0
  config.max_open_sockets = MOS;
  config.max_uri_handlers = 16;
  config.backlog_conn = 0;         // No waiting
  config.lru_purge_enable = false; // The first connection wins
  config.keep_alive_enable = true; // Persistent connections

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &hwinfo_get);
    httpd_register_uri_handler(server, &halt_put);
    httpd_register_uri_handler(server, &dyno_swp_post);
    httpd_register_uri_handler(server, &wifi_ssid_get);
    httpd_register_uri_handler(server, &wifi_pass_get);
    httpd_register_uri_handler(server, &wifi_cred_put);
    httpd_register_uri_handler(server, &dyno_seti_get);
    httpd_register_uri_handler(server, &dyno_seti_put);
    httpd_register_uri_handler(server, &dyno_save_put);
    httpd_register_uri_handler(server, &dyno_data_ws);
    return server;
  }

  ESP_LOGI(TAG, "Error starting server!");
  return NULL;
}