#define CONFIG_HTTPD_WS_SUPPORT 1

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "WebsocketHost.h"


#include "driver/gpio.h"
//#include "board.h"

const int WIFI_CONNECTED_EVENT = BIT0;
static httpd_data *server;

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
void WebsocketHost::ws_async_broadcast(void *arg)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.payload = (uint8_t *)arg;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.len = strlen((char *)arg);
    ws_pkt.final = true;

    //ESP_LOGI(TAG, "async_broadcast(%s)", (char *)arg);

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

bool WebsocketHost::consume(ObjMsgDataRef data)
{
    string str;
    data.get()->serializeJson(str);
    const char *json = str.c_str();
    int err = httpd_queue_work(server, ws_async_broadcast, strdup(json));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "consume(%s)=>%d", json, err);
    }
    return true;
}

//
// /ws (websocket) URI handler
//
esp_err_t WebsocketHost::ws_message_handler(httpd_req_t *req)
{
    WebsocketHost *host = (WebsocketHost *)req->user_ctx;

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(host->TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    char message[128] = { 0 };
    httpd_ws_frame_t ws_pkt;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(message));
    if (ret != ESP_OK) {
        ESP_LOGE(host->TAG, "httpd_ws_recv_frame failed with error %d", ret);
        return ret;
    }
    
    ObjMsgDataRef data = dataFactory.deserialize(host->origin_id, message);
    if (data) {
      host->produce(data);
    }
    return ret;
}


//
// / (http) URI handler
//
/***
 *      _      _    _          
 *     | |__  | |_ | |_  _ __  
 *     | '_ \ | __|| __|| '_ \ 
 *     | | | || |_ | |_ | |_) |
 *     |_| |_| \__| \__|| .__/ 
 *                      |_|    
extern "C" {
  extern const char *_binary_index_html_start, 
    *_binary_index_html_end,
    *_binary_favicon_ico_start, 
    *_binary_favicon_ico_end;
}
 */
/** Respond to http request for '/'
 */
esp_err_t WebsocketHost::http_root_handler(httpd_req_t *req)
{
    extern const unsigned char index_start[] asm("_binary_index_html_start");
    extern const unsigned char index_end[]   asm("_binary_index_html_end");
    const size_t index_size = (index_end - index_start);
    printf("index.html (%d bytes) from %p to %p\n", index_size, index_start, index_end);
//    printf("index.html (%d bytes) from %p to %p\n", _binary_index_html_end - _binary_index_html_start, _binary_index_html_start, _binary_index_html_end);
   // httpd_resp_send(req, (const char *)index_start, index_size);
/*    const size_t index_size = _binary_index_html_end - _binary_index_html_start;
    printf("index.html (%d bytes) from %p to %p\n", index_size, _binary_index_html_start, _binary_index_html_end);
    httpd_resp_send(req, _binary_index_html_start, index_size);
*/    return ESP_OK;
}


esp_err_t WebsocketHost::http_favicon_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_size = (favicon_end - favicon_start);
    printf("favicon.ico.html (%d bytes) from %p to %p\n", favicon_size, favicon_end);
 //   httpd_resp_send(req, (const char *)favicon_start, favicon_size);
//    const size_t favicon_size = _binary_favicon_ico_end - _binary_favicon_ico_start;
//    httpd_resp_send(req, _binary_favicon_ico_start, favicon_size);
    return ESP_OK;
}

//
// Server operation
//
httpd_handle_t WebsocketHost::start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &favicon);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void WebsocketHost::stop_webserver()
{
    // Stop the httpd server
    httpd_stop(server);
    server = NULL;
}

/*
 *      _     _   _           __         _  __ _ 
 *     | |__ | |_| |_ _ __   / /_      _(_)/ _(_)
 *     | '_ \| __| __| '_ \ / /\ \ /\ / / | |_| |
 *     | | | | |_| |_| |_) / /  \ V  V /| |  _| |
 *     |_| |_|\__|\__| .__/_/    \_/\_/ |_|_| |_|
 *                   |_|                         
 */

void WebsocketHost::wifi_event_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    WebsocketHost *host = (WebsocketHost *)arg;

    switch (event_id) {
      case WIFI_EVENT_STA_START:
        ESP_LOGI(host->TAG, "STA_START Connecting to the AP");
        esp_wifi_connect();
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(host->TAG, "Disconnected. Connecting to the AP again...");
        if (server) {
            ESP_LOGI(host->TAG, "Stopping webserver");
            stop_webserver();
       }
        esp_wifi_connect();
        break;
    }
}

void WebsocketHost::ip_connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    char buffer[30];
    WebsocketHost *host = (WebsocketHost *)arg;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(host->TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    sprintf(buffer, IPSTR, IP2STR(&event->ip_info.ip));
    host->produce(ObjMsgDataString::create(host->origin_id, "__my_ip__", buffer));

    if (server == NULL) {
        ESP_LOGI(host->TAG, "Starting webserver");
        host->start_webserver();
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
void WebsocketHost::wifi_scan(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    produce(ObjMsgDataString::create(origin_id, "__APSCAN__", "begin"));

    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        produce(ObjMsgDataString::create(origin_id, "__AP__", (const char *)ap_info[i].ssid));
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "Channel \t%d", ap_info[i].primary);
    }
    produce(ObjMsgDataString::create(origin_id, "__APSCAN__", "end"));
}

void WebsocketHost::blink_task(void *arg)
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

  bool WebsocketHost::start()
  {
    bool configured = false;
    // Init websocket handler
    memset(&ws, 0, sizeof(ws));
    ws.uri        = "/ws";
    ws.method     = HTTP_GET;
    ws.handler    = ws_message_handler;
    ws.user_ctx   = this;
    ws.is_websocket = true;

    memset(&root, 0, sizeof(root));
    root.uri        = "/";
    root.method     = HTTP_GET;
    root.handler    = http_root_handler;
    root.user_ctx   = this;
    root.is_websocket = false;

    memset(&favicon, 0, sizeof(favicon));
    favicon.uri        = "/favicon.ico";
    favicon.method     = HTTP_GET;
    favicon.handler    = http_favicon_handler;
    favicon.user_ctx   = this;
    favicon.is_websocket = false;

    TaskHandle_t blink_taskHandle;

    xTaskCreate(blink_task, "blink_task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE, 
      NULL, 10, &blink_taskHandle);
    
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
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_connect_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this));

   /* Get Wi-Fi Station configuration */
   // AFTER esp_wifi_init
    wifi_config_t wifi_cfg;
    esp_err_t err =  esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_cfg);
    if (err == ESP_OK) {
      if (strlen((const char *) wifi_cfg.sta.ssid)) {
        // CONFIGURED
        configured = true;
        printf("Configured %d: %s\n", configured, wifi_cfg.sta.ssid);
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

    vTaskDelete(blink_taskHandle);
#ifdef BUILTIN_LED      
    gpio_set_level(BUILTIN_LED, 0);
#endif    
  return true;
}
