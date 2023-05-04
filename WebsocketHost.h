#include <freertos/event_groups.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"

#include "ObjMsg.h"
#include <list>

class WebsocketHost : public ObjMsgHost
{
public:
  WebsocketHost(ObjMsgTransport &transport, uint16_t origin);
  bool start();
  bool consume(ObjMsgData *data);
  void stop_webserver();
  bool Add(const char *path, esp_err_t (*fn)(httpd_req_t *req), bool ws);

protected:

/* Signal Wi-Fi events on this event-group */
EventGroupHandle_t wifi_event_group;

std::list<httpd_uri_t> uris;

static void ws_async_broadcast(void *arg);
static esp_err_t ws_message_handler(httpd_req_t *req);
//static esp_err_t http_root_handler(httpd_req_t *req);
//static esp_err_t http_favicon_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data);
static void ip_connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data);
static void blink_task(void *arg);

/** Scan for wifi access points */
void wifi_scan(void);
};

