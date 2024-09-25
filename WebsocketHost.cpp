#define CONFIG_HTTPD_WS_SUPPORT 1

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_smartconfig.h"

#include "WebsocketHost.h"

#include "driver/gpio.h"

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

httpd_handle_t server;

/*       ___  _     _ __  __         _  _        _
 *      / _ \| |__ (_)  \/  |_____ _| || |___ __| |_
 *     | (_) | '_ \| | |\/| (_-< _` | __ / _ (_-<  _|
 *      \___/|_.__// |_|  |_/__|__, |_||_\___/__/\__|
 *               |__/          |___/
 */

WebsocketHost::WebsocketHost(ObjMsgTransport *transport, uint16_t origin,
                             gpio_num_t led)
    : ObjMsgHost(transport, "WEBSOCKET", origin)
{
  server = NULL;
  blinkTaskHandle = NULL;
  this->led = led;
  this->resetWifi = false;
  wifi_event_group = xEventGroupCreate();
}

bool WebsocketHost::Add(const char *path, esp_err_t (*fn)(httpd_req_t *req), bool ws)
{
  httpd_uri_t uri;
  memset(&uri, 0, sizeof(uri));
  uri.uri = path;
  uri.method = HTTP_GET;
  uri.handler = fn;
  uri.user_ctx = this;
  uri.is_websocket = ws;
  uris.push_back(uri);

  return true;
}

bool WebsocketHost::Start()
{
  isConnected = false;
  /* Initialize NVS partition */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    /* NVS partition was truncated
     * and needs to be erased */
    ESP_ERROR_CHECK(nvs_flash_erase());

    /* Retry nvs_flash_init */
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  // Init websocket handler
  Add("/ws", WebSockMsgHandler, true);

  if (led != GPIO_NUM_NC)
  {
    ledPattern = LED_PATTERN_CONNECTING;

    xTaskCreate(BlinkTask, "Blink Task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE,
                this, 10, &blinkTaskHandle);
  }

  /* Initialize network interface */
  ESP_ERROR_CHECK(esp_netif_init());

  /* Initialize the event loop */
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (resetWifi) {
    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    printf("esp_wifi_set_config = %d",
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  }

  /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
   * and re-start it upon connection as well as for provisioning
   */
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &IpConnectHandler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &ScEventHandler, this));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* Wait for Wi-Fi connection */
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

  if (blinkTaskHandle)
  {
    vTaskDelete(blinkTaskHandle);
    gpio_set_level(led, 0);
  }
  return true;
}

bool WebsocketHost::IsConnected()
{
  return isConnected;
}
void WebsocketHost::SetConnected(bool connected)
{
  isConnected = connected;
}

bool WebsocketHost::Consume(ObjMsgData *data)
{
  if (server)
  {
    string str;
    data->Serialize(str);
    const char *json = str.c_str();
    int err = httpd_queue_work(server, WebSockAsyncBroadcast, strdup(json));
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG.c_str(), "Consume(%s)=>%d", json, err);
    }
    return true;
  }
  else
  {
    return false;
  }
}

/*     __      __   _               _       _
 *     \ \    / /__| |__ ______  __| |_____| |_
 *      \ \/\/ / -_) '_ (_-< _ \/ _| / / -_)  _|
 *       \_/\_/\___|_.__/__|___/\__|_\_\___|\__|
 *
 */

//
// async send function broadcast worker)
//
void WebsocketHost::WebSockAsyncBroadcast(void *arg)
{
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.payload = (uint8_t *)arg;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.len = strlen((char *)arg);
  ws_pkt.final = true;

  // ESP_LOGI(TAG.c_str(), "async_broadcast(%s)", (char *)arg);

  static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
  size_t fds = max_clients;
  int client_fds[max_clients];

  esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
  if (ret != ESP_OK)
  {
    return;
  }

  for (int i = 0; i < fds; i++)
  {
    int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
    if (client_info == HTTPD_WS_CLIENT_WEBSOCKET)
    {
      int err = httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
      if (err)
      {
        ESP_LOGE("ASYNC-Broadcast", "error: %d)", err);
      }
    }
  }

  free(arg);
}

//
// /ws (websocket) URI handler
//
esp_err_t WebsocketHost::WebSockMsgHandler(httpd_req_t *req)
{
  WebsocketHost *host = (WebsocketHost *)req->user_ctx;

  if (req->method == HTTP_GET)
  {
    ESP_LOGI(host->TAG.c_str(), "Handshake done, the Websocket connection was opened");
    return ESP_OK;
  }

  char message[128] = {0};
  httpd_ws_frame_t ws_pkt;

  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t *)message;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(message));
  if (ret != ESP_OK)
  {
    ESP_LOGE(host->TAG.c_str(), "httpd_ws_recv_frame failed with error %d", ret);
    return ret;
  }
  host->Produce(message);

  return ret;
}

/*
 *      _      _    _
 *     | |__  | |_ | |_  _ __
 *     | '_ \ | __|| __|| '_ \
 *     | | | || |_ | |_ | |_) |
 *     |_| |_| \__| \__|| .__/
 *                      |_|
 */
//
// Server operation
//
httpd_handle_t WebsocketHost::StartWebserver(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(TAG.c_str(), "Starting HTTP server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK)
  {
    // Registering the ws handler
    for (std::list<httpd_uri_t>::iterator it = uris.begin(); it != uris.end(); ++it)
    {
      httpd_register_uri_handler(server, &(*it));
    }
    return server;
  }

  ESP_LOGE(TAG.c_str(), "Error starting server!");
  return NULL;
}

void WebsocketHost::StopWebserver()
{
  // Stop the httpd server
  httpd_stop(server);
  server = NULL;
}

#define RECONNECT_TRIES 5
int reconnectCounter = 0;

void WebsocketHost::IpConnectHandler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
  if (event_id == IP_EVENT_STA_GOT_IP)
  {
    char buffer[30];
    WebsocketHost *host = (WebsocketHost *)arg;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(host->TAG.c_str(), "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    sprintf(buffer, IPSTR, IP2STR(&event->ip_info.ip));
    host->Produce(ObjMsgDataString::Create(host->origin_id, "__WS_MY_IP__", buffer));

    host->SetConnected(true);
    reconnectCounter = 0;

    if (server == NULL)
    {
      ESP_LOGI(host->TAG.c_str(), "Starting webserver");
      host->StartWebserver();
    }
    /* Signal to continue execution */
    xEventGroupSetBits(host->wifi_event_group, CONNECTED_BIT);
  }
}

/*
 *               _   _ _
 *      _      _(_)/ _(_)
 *     \ \ /\ / / | |_| |
 *      \ V  V /| |  _| |
 *       \_/\_/ |_|_| |_|
 */

void WebsocketHost::WifiEventHandler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
  WebsocketHost *host = (WebsocketHost *)arg;

  switch (event_id)
  {
  case WIFI_EVENT_STA_START:
  {
    host->SetConnected(false);

    /* Get Wi-Fi Station configuration */
    bool configured = false;
    wifi_config_t wifi_cfg;
    esp_err_t err = esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_cfg);
    if (err == ESP_OK)
    {
      if (strlen((const char *)wifi_cfg.sta.ssid))
      {
        // CONFIGURED
        configured = true;
    	// ESP_LOGI(host->TAG.c_str(), "SSID:%s", wifi_cfg.sta.ssid);
    	// ESP_LOGI(host->TAG.c_str(), "PASSWORD:%s", wifi_cfg.sta.password);
      }
    }
    // TODO if configured connect, but if timeout waiting for CONNECT, start smartconfig
    // Think about doing this inside what is now SmartconfigTask
    if (configured)
    {
      ESP_LOGI(host->TAG.c_str(), "STA_START Connecting to the AP");
      host->ledPattern = LED_PATTERN_CONNECTING;
      esp_wifi_connect();
    }
    else
    {
      host->SmartConfigStart();
    }
    break;
  }
  case WIFI_EVENT_STA_DISCONNECTED:
    host->SetConnected(false);
    if (++reconnectCounter >= RECONNECT_TRIES) {
      reconnectCounter = 0;
      host->SmartConfigStart();
    }
    else {
      ESP_LOGW(host->TAG.c_str(), "Disconnected, trying to reconnect...");
      xEventGroupClearBits(host->wifi_event_group, CONNECTED_BIT);
      if (server)
      {
        ESP_LOGW(host->TAG.c_str(), "Stopping webserver");
        host->StopWebserver();
      }
      esp_wifi_connect();
    }
    break;
  }
}

/*      ___                _                __ _
 *     / __|_ __  __ _ _ _| |_ __ ___ _ _  / _(_)__ _
 *     \__ \ '  \/ _` | '_|  _/ _/ _ \ ' \|  _| / _` |
 *     |___/_|_|_\__,_|_|  \__\__\___/_||_|_| |_\__, |
 *                                              |___/
 */
void WebsocketHost::SmartConfigStart() {
    ledPattern = LED_PATTERN_PROVISIONING;
    xTaskCreate(SmartconfigTask, "smartconfig task", 4096, this, 3, NULL);
    Produce(ObjMsgDataString::Create(origin_id, "__WS_SMARTCONFIG__", "begin"));
}

void WebsocketHost::SmartconfigTask(void *arg)
{
  WebsocketHost *host = (WebsocketHost *)arg;
  EventBits_t uxBits;
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
  while (1)
  {
    uxBits = xEventGroupWaitBits(host->wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
    if (uxBits & CONNECTED_BIT)
    {
      ESP_LOGI(host->TAG.c_str(), "WiFi Connected to ap");
    }
    if (uxBits & ESPTOUCH_DONE_BIT)
    {
      ESP_LOGI(host->TAG.c_str(), "smartconfig over");
      esp_smartconfig_stop();
      host->Produce(ObjMsgDataString::Create(host->origin_id, "__WS_SMARTCONFIG__", "end"));
      vTaskDelete(NULL);
    }
  }
}

void WebsocketHost::ScEventHandler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
  WebsocketHost *host = (WebsocketHost *)arg;
  if (event_id == SC_EVENT_SCAN_DONE)
  {
    ESP_LOGI(host->TAG.c_str(), "Scan done");
  }
  else if (event_id == SC_EVENT_FOUND_CHANNEL)
  {
    ESP_LOGI(host->TAG.c_str(), "Found channel");
  }
  else if (event_id == SC_EVENT_GOT_SSID_PSWD)
  {
    host->ledPattern = LED_PATTERN_GOT_PW;

    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
    wifi_config_t wifi_config;
    uint8_t ssid[33] = {0};
    uint8_t password[65] = {0};
    uint8_t rvd_data[33] = {0};

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.bssid_set = evt->bssid_set;
    if (wifi_config.sta.bssid_set == true)
    {
      memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
    }

    memcpy(ssid, evt->ssid, sizeof(evt->ssid));
    memcpy(password, evt->password, sizeof(evt->password));
    ESP_LOGI(host->TAG.c_str(), "Smart config got SSID:%s, PASSWORD:%s", ssid, password);

    if (evt->type == SC_TYPE_ESPTOUCH_V2)
    {
      ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
      ESP_LOGI(host->TAG.c_str(), "RVD_DATA:");
      for (int i = 0; i < 33; i++)
      {
        printf("%02x ", rvd_data[i]);
      }
      printf("\n");
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
  }
  else if (event_id == SC_EVENT_SEND_ACK_DONE)
  {
    xEventGroupSetBits(host->wifi_event_group, ESPTOUCH_DONE_BIT);
  }
}

/*
 *      __  __ _
 *     |  \/  (_)___  ___
 *     | |\/| | / __|/ __|
 *     | |  | | \__ \ (__
 *     |_|  |_|_|___/\___|
 *
 */

#define DEFAULT_SCAN_LIST_SIZE 20

/* Initialize Wi-Fi as sta and set scan method */
void WebsocketHost::WifiScan(void)
{
  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_LOGI(TAG.c_str(), "Total APs scanned = %u", ap_count);
  Produce(ObjMsgDataString::Create(origin_id, "__WS_APSCAN__", "begin"));

  for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
  {
    Produce(ObjMsgDataString::Create(origin_id, "__WS_AP__", (const char *)ap_info[i].ssid));
    ESP_LOGI(TAG.c_str(), "SSID \t\t%s", ap_info[i].ssid);
    ESP_LOGI(TAG.c_str(), "RSSI \t\t%d", ap_info[i].rssi);
    ESP_LOGI(TAG.c_str(), "Channel \t%d", ap_info[i].primary);
  }
  Produce(ObjMsgDataString::Create(origin_id, "__WS_APSCAN__", "end"));
}

void WebsocketHost::BlinkTask(void *arg)
{
  WebsocketHost *host = (WebsocketHost *)arg;
  int counter = 0;
  for (;;)
  {
    //printf("blink %d: %04x\n", host->led, (host->ledPattern & 1 << (counter % 16))? 1 : 0);
    gpio_set_level(host->led, (host->ledPattern & 1 << (counter % 16))? 1 : 0);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    ++counter;
  }
}
