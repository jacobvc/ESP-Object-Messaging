#define CONFIG_HTTPD_WS_SUPPORT 1

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"

#include "WebsocketHost.h"


#include "driver/gpio.h"
//#include "board.h"

const int WIFI_CONNECTED_EVENT = BIT0;

httpd_handle_t server;

/*
 *                   _                    _    
 *     __      _____| |__  ___  ___   ___| | __
 *     \ \ /\ / / _ \ '_ \/ __|/ _ \ / __| |/ /
 *      \ V  V /  __/ |_) \__ \ (_) | (__|   < 
 *       \_/\_/ \___|_.__/|___/\___/ \___|_|\_\
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

    //ESP_LOGI(TAG.c_str(), "async_broadcast(%s)", (char *)arg);

    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            int err = httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
	        if (err) {
                ESP_LOGE("ASYNC-Broadcast", "error: %d)", err);
	        }
        }
    }

    free(arg);
}

bool WebsocketHost::Consume(ObjMsgData *data)
{
  if (server) {
    string str;
    data->Serialize(str);
    const char *json = str.c_str();
    int err = httpd_queue_work(server, WebSockAsyncBroadcast, strdup(json));
    if (err != ESP_OK) {
        ESP_LOGE(TAG.c_str(), "Consume(%s)=>%d", json, err);
    }
    return true;
  }
  else {
    return false;
  }
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

    char message[128] = { 0 };
    httpd_ws_frame_t ws_pkt;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(message));
    if (ret != ESP_OK) {
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
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        for (std::list<httpd_uri_t>::iterator it = uris.begin(); it != uris.end(); ++it){
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

/*
 *               _   _ _
 *      _      _(_)/ _(_)
 *     \ \ /\ / / | |_| |
 *      \ V  V /| |  _| |
 *       \_/\_/ |_|_| |_|
 */

void WebsocketHost::WifiEventHandler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    WebsocketHost *host = (WebsocketHost *)arg;

    switch (event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(host->TAG.c_str(), "STA_START Connecting to the AP");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(host->TAG.c_str(), "Disconnected. Connecting to the AP again...");
        if (server) {
            ESP_LOGW(host->TAG.c_str(), "Stopping webserver");
            host->StopWebserver();
       }
        esp_wifi_connect();
        break;
    }
}

void WebsocketHost::IpConnectHandler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    char buffer[30];
    WebsocketHost *host = (WebsocketHost *)arg;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(host->TAG.c_str(), "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    sprintf(buffer, IPSTR, IP2STR(&event->ip_info.ip));
    host->Produce(ObjMsgDataString::Create(host->origin_id, "__my_ip__", buffer));

    if (server == NULL) {
        ESP_LOGI(host->TAG.c_str(), "Starting webserver");
        host->StartWebserver();
    }
    /* Signal to continue execution */
    xEventGroupSetBits(host->wifi_event_group, WIFI_CONNECTED_EVENT);
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
    Produce(ObjMsgDataString::Create(origin_id, "__APSCAN__", "begin"));

    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        Produce(ObjMsgDataString::Create(origin_id, "__AP__", (const char *)ap_info[i].ssid));
        ESP_LOGI(TAG.c_str(), "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG.c_str(), "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG.c_str(), "Channel \t%d", ap_info[i].primary);
    }
    Produce(ObjMsgDataString::Create(origin_id, "__APSCAN__", "end"));
}

void WebsocketHost::BlinkTask(void *arg)
{
    for (;;) {
#ifdef BUILTIN_LED      
      gpio_set_level(BUILTIN_LED, 1);
#endif
      vTaskDelay(200 / portTICK_PERIOD_MS);
#ifdef BUILTIN_LED      
      gpio_set_level(BUILTIN_LED, 0);
#endif
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}



WebsocketHost::WebsocketHost(ObjMsgTransport &transport, uint16_t origin)
      : ObjMsgHost(transport, "WEBSOCKET", origin)
  {
    server = NULL;
    wifi_event_group = xEventGroupCreate();

    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }
  }

  bool WebsocketHost::Add(const char *path, esp_err_t (*fn)(httpd_req_t *req), bool ws)
  {
    httpd_uri_t uri;
    memset(&uri, 0, sizeof(uri));
    uri.uri        = path;
    uri.method     = HTTP_GET;
    uri.handler    = fn;
    uri.user_ctx   = this;
    uri.is_websocket = ws;
    uris.push_back(uri);

    return true;
  }

  bool WebsocketHost::Start()
  {
    bool configured = false;
    // Init websocket handler
    Add("/ws", WebSockMsgHandler, true);

    TaskHandle_t blinkTaskHandle;

    xTaskCreate(BlinkTask, "Blink Task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE, 
      NULL, 10, &blinkTaskHandle);
    
    /* Initialize network interface */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &IpConnectHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, this));

   /* Get Wi-Fi Station configuration */
   // AFTER esp_wifi_init
    wifi_config_t wifi_cfg;
    esp_err_t err =  esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_cfg);
    if (err == ESP_OK) {
      if (strlen((const char *) wifi_cfg.sta.ssid)) {
        // CONFIGURED
        configured = true;
      }
    }

    if (configured) {
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_start());
    }
    else {
      extern void WiFiProvisionInit();
      WiFiProvisionInit();
    }


    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    vTaskDelete(blinkTaskHandle);
#ifdef BUILTIN_LED      
    gpio_set_level(BUILTIN_LED, 0);
#endif    
  return true;
}
