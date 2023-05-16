/** @file
 */
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include <esp_log.h>

#include <string>
#include <unordered_map>
#include <list>
#include <memory>

#include "cjson.h"
#include "ObjMsgData.h"

using namespace std;

/** Message queue macros
 */
#define MSG_QUEUE_MAX_DEPTH 10

class ObjMsgTransport;

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
  const string TAG;
  const uint16_t origin_id;

  /** Constructor
   *
   */
  ObjMsgHost(ObjMsgTransport &transport, const char *tag, uint16_t origin)
      : transport(transport), TAG(tag), origin_id(origin) {}

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
  virtual BaseType_t Produce(ObjMsgDataRef data);

  /** Produce data created by parsing a JSON encoded message
   *
   * WARNING - for now, uses global dataFactory
   *
   * <returns> true if successfully produced / else false
   */
  virtual BaseType_t Produce(const char *message)
  {
    ObjMsgDataRef data = ObjMsgData::Deserialize(origin_id, message);
    if (data)
    {
      return Produce(data);
    }
    return false;
  }

  /** Start the change detection operations */
  virtual bool Start() = 0;
};

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
    ObjMessage(ObjMsgDataRef dataRef) { _p = dataRef; }
    ~ObjMessage()
    { /* ESP_LOGI(CORE_TAG, "ObjMessage destructed"); */
    }
    ObjMsgDataRef &dataRef() { return _p; }

  private:
    ObjMsgDataRef _p;
  };

public:
  ObjMsgTransport(uint16_t message_queue_depth)
  {
    message_queue = xQueueCreate(message_queue_depth, sizeof(ObjMessage *));
  }
  BaseType_t Send(ObjMsgDataRef dataRef)
  {
    // Create message object to send shared_ptr data
    ObjMessage *msg = new ObjMessage(dataRef);
    // and send it
    return xQueueSend(message_queue, &msg, 0);
  }

  bool AddForward(ObjMsgHost *fwd)
  {
    forwards.push_back(fwd);
    return true;
  }

  void Forward(ObjMsgData *data)
  {
    for (list<ObjMsgHost *>::iterator it = forwards.begin(); it != forwards.end(); ++it)
    {
      if (!data->IsFrom((*it)->origin_id))
      {
        (*it)->Consume(data);
      }
    }
  }

  BaseType_t Receive(ObjMsgDataRef &dataRef, TickType_t xTicksToWait)
  {
    ObjMessage *msg;
    BaseType_t result = xQueueReceive(message_queue, &msg, xTicksToWait);
    if (result)
    {
      // Received a message. Give caller reference to its data and delete the shared_ptr message object
      dataRef = msg->dataRef();
      Forward(dataRef.get());
      delete msg;
    }
    // Return message reception result
    return result;
  }

protected:
  list<ObjMsgHost *> forwards;
};
