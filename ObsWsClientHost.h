/**
 * Requires esp_websocket_client. You will need to add this to your
 * application's idf_component.yml
  espressif/esp_websocket_client:
    version: "^1.1.0"
 */

#include <cJSON.h>
#include "esp_websocket_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"

#include "ObjMsg.h"

#define RPCVERSION "1"
#define REQUEST_ID_SEED "ThisRequest"
#define NO_DATA_TIMEOUT_SEC 5
#define CONNECT_TIMEOUT_SEC 5

enum ObsOpcodes {
  Hello = 0,
  Identify = 1,	       /**< The message sent by a newly connected client to obs-websocket in response to a `Hello` */
  Identified = 2,      /**< The response sent by obs-websocket to a client after it has successfully identified with obs-websocket. */
  Reidentify = 3,	     /**< The message sent by an already-identified client to update identification parameters. */
  Event = 5,           /*** The message sent by obs-websocket containing an event payload. */
  Request = 6,         /**< The message sent by a client to obs-websocket to perform a request. */
  RequestResponse = 7, /**< The message sent by obs-websocket in response to a particular request from a client. */
  RequestBatch = 8,    /**< The message sent by a client to obs-websocket to perform a batch of requests. */
  RequestBatchResponse = 9,/**< The message sent by obs-websocket in response to a particular batch of requests from a client. */
};

class ObsWsClientHost : public ObjMsgHost
{
protected:
  esp_websocket_client_config_t websocket_cfg;
  int requestCount;
  esp_websocket_client_handle_t client;
  SemaphoreHandle_t connect_sema;
  bool identified;
  string assembly;

public:
  ObsWsClientHost(ObjMsgTransport* transport, uint16_t origin, const char* uri)
    : ObjMsgHost(transport, "ObsWsClientHost", origin),
    websocket_cfg{}
  {
    websocket_cfg.uri = uri;
    websocket_cfg.network_timeout_ms = 1000;
    websocket_cfg.reconnect_timeout_ms = 3000;

    requestCount = 0;
    identified = false;

    connect_sema = xSemaphoreCreateBinary();

    client = esp_websocket_client_init(&websocket_cfg);

    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
      websocket_event_handler, this);
  }

  ~ObsWsClientHost() {
    esp_websocket_client_destroy(client);
  }

  bool Start()
  {
    ESP_LOGI(TAG.c_str(), "Connecting to %s...", websocket_cfg.uri);

    esp_websocket_client_start(client);

    if (xSemaphoreTake(connect_sema, CONNECT_TIMEOUT_SEC * 1000 / portTICK_PERIOD_MS)) {
      Identify();
      return true;
    }
    else {
      return false;
    }
  }

  void Stop()
  {
    esp_websocket_client_close(client, portMAX_DELAY);
    ESP_LOGI(TAG.c_str(), "Websocket Stopped");
  }

  esp_err_t Send(cJSON* msg) {
    esp_err_t err = ESP_OK;
    char* json = cJSON_Print(msg);
    ESP_LOGD(TAG.c_str(), "Sending %s", json);
    if (esp_websocket_client_is_connected(client)) {
      err = esp_websocket_client_send_text(client, json, strlen(json), portMAX_DELAY);
    }
    else {
      err = ESP_FAIL;
    }
    return err;
  }

  //
  // Protocol Level Messages
  //

  esp_err_t Identify() {
    char identify[] = "{ \"op\": 1, \"d\": { \"rpcVersion\": " RPCVERSION " } }";
    ESP_LOGD(TAG.c_str(), "Sending Identify %s", identify);
    return esp_websocket_client_send_text(client, identify, sizeof(identify), portMAX_DELAY);
  }

  esp_err_t GetVersion() {
    return Request("GetVersion");
  }

  //
  // Event handlers
  //
  static void websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
  {
    ObsWsClientHost* ws = (ObsWsClientHost*)handler_args;
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {

    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGI(ws->TAG.c_str(), "WEBSOCKET_EVENT_CONNECTED");
      xSemaphoreGive(ws->connect_sema);
      break;

    case WEBSOCKET_EVENT_DISCONNECTED:
      ESP_LOGI(ws->TAG.c_str(), "WEBSOCKET_EVENT_DISCONNECTED");
      log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
      if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
      }
      break;

    case WEBSOCKET_EVENT_DATA:
      ESP_LOGI(ws->TAG.c_str(), "WEBSOCKET_EVENT_DATA payload length=%d, data_len=%d, current payload offset=%d",
        data->payload_len, data->data_len, data->payload_offset);
      if (data->payload_len == data->payload_offset + data->data_len) {
        // Message complete
        if (data->payload_offset != 0) {
          ws->assembly.append(data->data_ptr, data->data_len);
        }
        switch (data->op_code) {
        case WS_TRANSPORT_OPCODES_CONT:
          break;
        case WS_TRANSPORT_OPCODES_TEXT:
        {
          // If received data contains json structure it is valid JSON
          cJSON* root;
          if (ws->assembly.length() > 0) {
            root = cJSON_Parse(ws->assembly.c_str());
          }
          else {
            root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
          }
          ws->assembly = ""; // Done with assmebly contents
          if (root) {
            cJSON *jOp = cJSON_GetObjectItem(root, "op");
            int op = cJSON_GetNumberValue(jOp);
            switch (op) {
              case Hello:
                ESP_LOGI(ws->TAG.c_str(), "Hello op.");
                break;
              case ObsOpcodes::Identify:
                ESP_LOGE(ws->TAG.c_str(), "UNEXPECTED Identify op.");
                break;
              case Identified:
                ESP_LOGI(ws->TAG.c_str(), "Identified op.");
                break;
              case Reidentify:
                ESP_LOGI(ws->TAG.c_str(), "Reidentify op.");
                break;
              case Event:
                ESP_LOGI(ws->TAG.c_str(), "Event op.");
                break;
              case ObsOpcodes::Request:
                ESP_LOGE(ws->TAG.c_str(), "UNEXPECTED Request op.");
                break;
              case RequestResponse:
                ESP_LOGI(ws->TAG.c_str(), "RequestResponse op.");
                break;
              case RequestBatch:
                ESP_LOGE(ws->TAG.c_str(), "UNEXPECTED RequestBatch op.");
                break;
              case RequestBatchResponse:
                ESP_LOGE(ws->TAG.c_str(), "UNEXPECTED RequestBatchResponse op.");
                break;
            }
              //      cJSON *elem = cJSON_GetArrayItem(root, i);
              //      cJSON *id = cJSON_GetObjectItem(elem, "id");
              //      cJSON *name = cJSON_GetObjectItem(elem, "name");
              //      ESP_LOGW(ws->TAG.c_str(), "Json={'id': '%s', 'name': '%s'}", id->valuestring, name->valuestring);
            ESP_LOGI(ws->TAG.c_str(), "Received JSON=%s", cJSON_Print(root));

            cJSON_Delete(root);
          }
          else {
            ESP_LOGE(ws->TAG.c_str(), "Received NON-JSON=%.*s", data->data_len, (char*)data->data_ptr);
          }
        }
        break;
        case WS_TRANSPORT_OPCODES_BINARY:
          ESP_LOGI(ws->TAG.c_str(), "BINARY not implemented.");
          break;
        case WS_TRANSPORT_OPCODES_CLOSE:
          ESP_LOGW(ws->TAG.c_str(), "Received CLOSE with code=%d",
            256 * data->data_ptr[0] + data->data_ptr[1]);
          break;
        case WS_TRANSPORT_OPCODES_PING:
          ESP_LOGI(ws->TAG.c_str(), "Received PING");
          break;
        case WS_TRANSPORT_OPCODES_PONG:
          ESP_LOGI(ws->TAG.c_str(), "Received PONG");
          break;
        case WS_TRANSPORT_OPCODES_FIN:
          ESP_LOGI(ws->TAG.c_str(), "Received FIN");
          break;
        default:
          ESP_LOGW(ws->TAG.c_str(), "Received Unexpected opcode %d", data->op_code);
          break;
        }
      }
      else {
        if (ws->assembly.length() != data->payload_offset) {
          ESP_LOGE(ws->TAG.c_str(), "Assembly error, resetting assmebly");
          ws->assembly = "";
        }
        else {
          ESP_LOGI(ws->TAG.c_str(), "Partial data ... assembling");
          ws->assembly.append(data->data_ptr, data->data_len);
        }
      }
      break;

    case WEBSOCKET_EVENT_ERROR:
      ESP_LOGD(ws->TAG.c_str(), "WEBSOCKET_EVENT_ERROR");
      log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
      if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
      }
      break;
    }
  }

protected:

  esp_err_t Request(const char* reqName, cJSON* reqData = NULL) {
    ESP_LOGI(TAG.c_str(), "Requesting %s", reqName);
    cJSON* msg = cJSON_CreateObject();
    cJSON* d = cJSON_CreateObject();

    cJSON_AddNumberToObject(msg, "op", 6);

    cJSON_AddStringToObject(d, "requestType", reqName);
    cJSON_AddItemToObject(msg, "d", d);
    char requestId[30];
    sprintf(requestId, "%s-%d", REQUEST_ID_SEED, ++requestCount);
    cJSON_AddStringToObject(d, "requestId", requestId);
    if (reqData != NULL) {
      cJSON_AddItemToObject(d, "requestData", reqData);
    }

    esp_err_t result = Send(msg);
    cJSON_Delete(msg);
    return result;
  }

  static void log_error_if_nonzero(const char* message, int error_code)
  {
    if (error_code != 0) {
      ESP_LOGE("ObsWsClientHost", "Last error %s: 0x%x", message, error_code);
    }
  }
};