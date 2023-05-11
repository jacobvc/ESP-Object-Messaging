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
  bool Start();
  bool Consume(ObjMsgData *data);
  void StopWebserver();
  bool Add(const char *path, esp_err_t (*fn)(httpd_req_t *req), bool ws);

protected:
  /* Signal Wi-Fi events on this event-group */
  EventGroupHandle_t wifi_event_group;

  std::list<httpd_uri_t> uris;

  static void WebSockAsyncBroadcast(void *arg);
  static esp_err_t WebSockMsgHandler(httpd_req_t *req);
  httpd_handle_t StartWebserver(void);
  static void WifiEventHandler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
  static void IpConnectHandler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
  static void BlinkTask(void *arg);

  /** Scan for wifi access points */
  void WifiScan(void);
};
