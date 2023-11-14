#include <freertos/event_groups.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#include "ObjMsg.h"
#include <list>

// LED Patterns; 16 member sequence of LED On/Off bits, applied at 250ms interval
#define LED_PATTERN_CONNECTING 0x3333    // 1 sec beat
#define LED_PATTERN_PROVISIONING 0x55ee // 2 long, 4 short      0x5555  // 1/2 sec beat
#define LED_PATTERN_GOT_PW  0x5555  // 1/2 sec beat

/** WiFi / httpd / Websocket ObjMsgHost with Smartconfig commissioning
 * 
 * 
 */
class WebsocketHost : public ObjMsgHost
{
public:
  /// @brief Set true before start() to reset WiFi AP configuration
  bool resetWifi;
  /// Constructor, specifying transport object and origin, and optionally a LED GPIO number
  /// and a directive to reset the wifi credentials
  /// @param transport: transport object
  /// @param origin: this host's origin id
  /// @param led: (Optional) GPIO number for LED
  WebsocketHost(ObjMsgTransport *transport, uint16_t origin,
    gpio_num_t led = GPIO_NUM_NC);

  /// Add handler supported at specified path, to be supported by httpd or ws as specified
  ///
  /// Files are normally embedded in the binary using EMBED_FILES in a CMakeLists.txt
  /// @param path: speciied path
  /// @param fn: the handler function
  /// @param ws: host type - true == ws, false = httpd
  /// @return bool - Successfully added
  bool Add(const char *path, esp_err_t (*fn)(httpd_req_t *req), bool ws);
  bool Start();
  bool Consume(ObjMsgData *data);

  /// Scan for wifi access points
  void WifiScan(void);

protected:
  /* Signal Wi-Fi events on this event-group */
  EventGroupHandle_t wifi_event_group;
  TaskHandle_t blinkTaskHandle;
  gpio_num_t led;
  int ledPattern;

  std::list<httpd_uri_t> uris;

  // Websocket
  static void WebSockAsyncBroadcast(void *arg);
  static esp_err_t WebSockMsgHandler(httpd_req_t *req);

  // http
  httpd_handle_t StartWebserver(void);
  void StopWebserver();
  static void IpConnectHandler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);

  // Wifi
  static void WifiEventHandler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
  // Smartconfig                                 
  static void SmartconfigTask(void * parm);
  static void ScEventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

  // Misc
  static void BlinkTask(void *arg);
};
