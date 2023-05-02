/** @file
 *
 * ObjMsg is intended to support relativly simple integration of
 * components into an application that utilizes external access via connectivity that
 * normally is text based such as http, etc, and other messaging that typically might
 * utilize json.
 *
 * ObjMsgTransport intantiates a freertos 'message_queue' which exchanges
 * ObjMessage's, carrying ObjMsgData.
 *
 * ObjMsgHost implements produce() which attaches ObjMsgData to a new ObjMessage and
 * sends it using ObjMsgTransport.
 *
 * The application receives produced ObjMsgData by calling transport.receive()
 * which waits up to a specified time for a message, deletes the message (after
 * removing the attached data), and invokes consume() on intended destinations.
 *
 * Each library component may produce content, and sends it by invoking this->produce()
 * identifying itself as origin and passing the produced content as a name / value pair.
 *
 * Each component may also consume content, and  must support that by implementing
 * consume() which is typically called by the application (controller) to
 * deliver ObjMsgData.
 *
 * The ObjMsgDataFactory supports creation of a ObjMsgData object based on recevied data.
 * The initial implementation supports only JSON data. Each endpoint supporting
 * object creation must register with the factory, this example (for data of
 * type ObjMsgDataInt32):
 *
 *    dataFactory.registerClass(my_origin, "my_name", ObjMsgDataInt32::create);
 *
 * Will create a ObjMsgDataInt32 object when data.get() is called for
 * JSON data containing "name":"my_data".
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

/**
 *      __  __
 *     |  \/  |___ ________ _ __ _ ___
 *     | |\/| / -_|_-<_-< _` / _` / -_)
 *     |_|  |_\___/__/__|__,_\__, \___|
 *                           |___/
 */

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
  ObjMessage(ObjMsgDataRef data)
  {
    _p = data;
    // ESP_LOGI(CORE_TAG, "ObjMessage constructed");
  }

  ~ObjMessage()
  {
    // ESP_LOGI(CORE_TAG, "ObjMessage destructed");
  }

  ObjMsgDataRef &data()
  {
    return _p;
  }

private:
  ObjMsgDataRef _p;
};

/**
 *      ___       _          ___        _
 *     |   \ __ _| |_ __ _  | __|_ _ __| |_ ___ _ _ _  _
 *     | |) / _` |  _/ _` | | _/ _` / _|  _/ _ \ '_| || |
 *     |___/\__,_|\__\__,_| |_|\__,_\__|\__\___/_|  \_, |
 *                                                  |__/
 */

/** This Factory creates ObjMsgDataRef from JSON
 * data into for specified data 'name'.
 */
class ObjMsgDataFactory
{
  unordered_map<string, ObjMsgDataRef (*)(uint16_t, char const *)> dataClasses;

public:
  /** register object creator function 'fn' to create object for endpoint 'name' */
  bool registerClass(uint16_t origin, string name, ObjMsgDataRef (*fn)(uint16_t, char const *))
  {
    return dataClasses.insert(make_pair(name, fn)).second;
  }
  /** Create ObjMsgData object for endpoint 'name' */
  ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    ObjMsgDataRef (*fn)(uint16_t, char const *) = dataClasses[name];
    if (fn)
      return fn(origin, name);
    return NULL;
  }
  /** Create ObjMsgData object and populate it based on 'json' content */
  ObjMsgDataRef deserialize(uint16_t origin, char const *json)
  {
    ObjMsgDataRef data = NULL;

    cJSON *root = cJSON_Parse(json);
    if (root)
    {
      cJSON *jsonName = cJSON_GetObjectItemCaseSensitive(root, "name");
      if (jsonName)
      {
        data = create(origin, cJSON_GetStringValue(jsonName));
        if (data)
        {
          if (data.get()->deserializeValue(root))
          {
            cJSON_Delete(root);
            return data;
          }
          else
          {
            ESP_LOGE("deserialize", "Unable to parse value: %s", json);
          }
        }
        else
        {
          ESP_LOGE("deserialize", "No class registered for: %s", cJSON_GetStringValue(jsonName));
        }
      }
      else
      {
        ESP_LOGE("deserialize", "JSON has no name field: %s", json);
      }
      cJSON_Delete(root);
    }
    else
    {
      ESP_LOGE("deserialize", "JSON malformed: %s", json);
    }

    return NULL;
  }
};

extern ObjMsgDataFactory dataFactory; /**< THE data factory */

/**
 *      _____                               _
 *     |_   _| _ __ _ _ _  ____ __  ___ _ _| |_
 *       | || '_/ _` | ' \(_-< '_ \/ _ \ '_|  _|
 *       |_||_| \__,_|_||_/__/ .__/\___/_|  \__|
 *                           |_|
 */

class ObjMsgTransport
{
  QueueHandle_t message_queue = NULL;

public:
  ObjMsgTransport(uint16_t message_queue_depth);
  BaseType_t send(ObjMsgDataRef data);
  BaseType_t receive(ObjMsgDataRef &data, TickType_t xTicksToWait);
};

/**
 *      _  _        _
 *     | || |___ __| |_
 *     | __ / _ (_-<  _|
 *     |_||_\___/__/\__|
 *
 */

enum ObjMsgSample
{
  POLLING,
  CHANGE_EVENT
};

class ObjMsgHost
{
protected:
  ObjMsgTransport &transport;

public:
  const char *TAG;
  uint16_t origin_id;

  ObjMsgHost(ObjMsgTransport &transport, const char *tag,
             uint16_t origin) : transport(transport)
  {
    TAG = tag;
    origin_id = origin;
  }
  virtual bool consume(ObjMsgData *data) { return false; }
  virtual BaseType_t produce(ObjMsgDataRef data)
  {
    return transport.send(data);
  }
  virtual bool start() = 0;
};
