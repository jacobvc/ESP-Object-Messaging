/** @file
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include <esp_log.h>

#include <string>
#include <unordered_map>
#include <memory>

#include "cjson.h"
#include "ObjMsgData.h"

using namespace std;

/** Message queue macros
 */
#define MSG_QUEUE_MAX_DEPTH 10
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/*
 *      ___       _          ___        _
 *     |   \ __ _| |_ __ _  | __|_ _ __| |_ ___ _ _ _  _
 *     | |) / _` |  _/ _` | | _/ _` / _|  _/ _ \ '_| || |
 *     |___/\__,_|\__\__,_| |_|\__,_\__|\__\___/_|  \_, |
 *                                                  |__/
 */

/** Factory to create ObjMsgDataRef from JSON data for registered data 'name'.
 */
class ObjMsgDataFactory
{
  unordered_map<string, ObjMsgDataRef (*)(uint16_t, char const *)> dataClasses;

public:
  /** register object creator function 'fn' to create object for endpoint 'name' */
  bool RegisterClass(uint16_t origin, string name, ObjMsgDataRef (*fn)(uint16_t, char const *))
  {
    return dataClasses.insert(make_pair(name, fn)).second;
  }
  /** Create ObjMsgDataRef object for endpoint 'name' */
  ObjMsgDataRef Create(uint16_t origin, char const *name)
  {
    ObjMsgDataRef (*fn)(uint16_t, char const *) = dataClasses[name];
    if (fn)
      return fn(origin, name);
    return NULL;
  }
  /** Create ObjMsgDataRef object and populate it based on 'json' content */
  ObjMsgDataRef Deserialize(uint16_t origin, char const *json)
  {
    ObjMsgDataRef data = NULL;

    cJSON *root = cJSON_Parse(json);
    if (root)
    {
      cJSON *jsonName = cJSON_GetObjectItemCaseSensitive(root, "name");
      if (jsonName)
      {
        data = Create(origin, cJSON_GetStringValue(jsonName));
        if (data)
        {
          if (data.get()->DeserializeValue(root))
          {
            cJSON_Delete(root);
            return data;
          }
          else
          {
            ESP_LOGE("Deserialize", "Unable to parse value: %s", json);
          }
        }
        else
        {
          printf("Create(%d, %s, %s)\n", origin, cJSON_GetStringValue(jsonName),
                 cJSON_Print(cJSON_GetObjectItem(root, "value")));
          // If not registered, deliver as JSON object carried ib string
          data = ObjMsgDataString::Create(origin, cJSON_GetStringValue(jsonName),
                                          cJSON_Print(cJSON_GetObjectItem(root, "value")), true);
          ESP_LOGI("Deserialize", "No class registered for: %s", cJSON_GetStringValue(jsonName));

          cJSON_Delete(root);
          return data;
        }
      }
      else
      {
        ESP_LOGE("Deserialize", "JSON has no name field: %s", json);
      }
      cJSON_Delete(root);
    }
    else
    {
      ESP_LOGE("Deserialize", "JSON malformed: %s", json);
    }

    return NULL;
  }
};

extern ObjMsgDataFactory dataFactory; /**< THE data factory */

/*
 *      _____                               _
 *     |_   _| _ __ _ _ _  ____ __  ___ _ _| |_
 *       | || '_/ _` | ' \(_-< '_ \/ _ \ '_|  _|
 *       |_||_| \__,_|_||_/__/ .__/\___/_|  \__|
 *                           |_|
 */

/**
 *
 */
class ObjMsgTransport
{
  QueueHandle_t message_queue = NULL;
  /** Class used to convey ObjMsgData messages.
   *
   * ObjMessage implementation creates a
   * new ObjMessage to be sent carrying a reference to requested
   * ObjMsgData (ObjMsgDataRef), then deletes that reference upon reception.
   *
   * The lifetime of the ObjMsgData is managed by virtue
   * of it's shared pointer.
   */
  class ObjMessage
  {
  public:
    ObjMessage(ObjMsgDataRef data) { _p = data;  }
    ~ObjMessage()  { /* ESP_LOGI(CORE_TAG, "ObjMessage destructed"); */ }
    ObjMsgDataRef &data() { return _p; }
  private:
    ObjMsgDataRef _p;
  };

public:
  ObjMsgTransport(uint16_t message_queue_depth);
  BaseType_t Send(ObjMsgDataRef data);
  BaseType_t Receive(ObjMsgDataRef &data, TickType_t xTicksToWait);
};

/*
 *      _  _        _
 *     | || |___ __| |_
 *     | __ / _ (_-<  _|
 *     |_||_\___/__/\__|
 *
 */

/** Sampling configuration.
 *  For CHANGE_EVENT, the host implements a mechanism to detect
 *  changes and Produce() a message when that occurs
 */
enum ObjMsgSample
{
  POLLING,     /**< Manual sampling */
  CHANGE_EVENT /**< Detect changes and Produce() message upon change */
};

/** virtual base class - Host a resource and Produce() / Consume() it's content
 *
 */
class ObjMsgHost
{
protected:
  ObjMsgTransport &transport; /** transport used to send content */

public:
  const char *TAG;
  uint16_t origin_id;

  /** Constructor
   *
   */
  ObjMsgHost(ObjMsgTransport &transport, const char *tag, uint16_t origin)
      : transport(transport)
  {
    TAG = tag;
    origin_id = origin;
  }

  /** Consume provided data
   *
   * Returns true if successfully consumed, false if not registered as consumer or unab le to use data
   */
  virtual bool Consume(ObjMsgData *data) { return false; }

  /** Produce provided data
   *
   * use transport to Send provided data
   *
   * <returns> true if successfully produced / else false
   */
  virtual BaseType_t Produce(ObjMsgDataRef data)
  {
    return transport.Send(data);
  }

  /** Produce data created by parsing a JSON encoded message
   *
   * WARNING - for now, uses global dataFactory
   *
   * <returns> true if successfully produced / else false
   */
  virtual BaseType_t Produce(const char *message)
  {
    ObjMsgDataRef data = dataFactory.Deserialize(origin_id, message);
    if (data)
    {
      return Produce(data);
    }
    return false;
  }

  /** Start the change detection operations */
  virtual bool Start() = 0;
};
