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

#include "cJSON.h"
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

/// Sampling configuration.
/// For CHANGE_EVENT, the host implements a mechanism to detect
/// changes and Produce() a message when that occurs
enum ObjMsgSample
{
  POLLING,     /**< Manual sampling */
  CHANGE_EVENT /**< Detect changes and Produce() message upon change */
};

/// virtual base class - Host a resource and Produce() / Consume() it's content
class ObjMsgHost
{
protected:
  ObjMsgTransport *transport; ///< transport used to send content
  const string TAG; ///< TAG for log messages
  const uint16_t origin_id; ///< Name for log messages from ths instance

public:
  /// Constructor , specifying transport object, tag and origin
  /// @param transport: Transport object
  /// @param tag: Name for log messages
  /// @param origin: Origin ID for this host
  ObjMsgHost(ObjMsgTransport *transport, const char *tag, uint16_t origin)
      : transport(transport), TAG(tag), origin_id(origin) {}

  /// Consume provided data
  ///
  /// @param data - data to consume
  /// @return true if successfully consumed, false if not registered as consumer or unable to use data
  virtual bool Consume(ObjMsgData *data) { return false; }

  /// Produce provided data
  ///
  /// use transport to Send provided data
  /// @param data - data to send
  /// @return boolean success
  virtual bool Produce(ObjMsgDataRef data); // Implemented in ObjMsgDataFactory.cpp

  /// Produce data created by parsing a JSON encoded message
  ///
  /// WARNING - for now, uses global dataFactory
  ///
  /// @param message message to parse
  /// @return boolean success
  virtual bool Produce(const char *message)
  {
    ObjMsgDataRef data = ObjMsgData::Deserialize(origin_id, message);
    if (data)
    {
      return Produce(data) ? true : false;
    }
    return false;
  }

  /// Start the change detection operations
  /// @return boolean successfully started
  virtual bool Start() = 0;

  /// Get the origin ID for this host
  /// @return the origin ID
  uint16_t GetOrigin() { return origin_id; }
};

/*
 *      _____                               _
 *     |_   _| _ __ _ _ _  ____ __  ___ _ _| |_
 *       | || '_/ _` | ' \(_-< '_ \/ _ \ '_|  _|
 *       |_||_| \__,_|_||_/__/ .__/\___/_|  \__|
 *                           |_|
 */

/// Send / Receive messages
class ObjMsgTransport
{
  QueueHandle_t message_queue = NULL;
  /// Class used to convey ObjMsgData messages.
  ///
  /// ObjMessage implementation creates a
  /// new ObjMessage to be sent carrying a reference to requested
  /// ObjMsgData (ObjMsgDataRef), then deletes that reference upon reception.
  ///
  /// The lifetime of the ObjMsgData is managed by virtue
  /// of it's shared pointer.
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
  /// Constructor
  /// @param message_queue_depth
  ObjMsgTransport(uint16_t message_queue_depth)
  {
    message_queue = xQueueCreate(message_queue_depth, sizeof(ObjMessage *));
  }

  /// Create a message containing 'dataref' and send to 'message_queue'
  /// @param dataRef: Data to send
  /// @return boolean success
  bool Send(ObjMsgDataRef dataRef)
  {
    // Create message object to send shared_ptr data
    ObjMessage *msg = new ObjMessage(dataRef);
    // and send it
    bool result = xQueueSend(message_queue, &msg, 0) ? true:false;
    if (!result) {
      ESP_LOGW("TRANSPORT", "Message Q Overflow");
     delete msg;
    }
    return result;
  }

  /// Add 'fwd' to the forwards list
  /// @param fwd: host to receive forwards
  /// @return true if successful
  bool AddForward(int fromOrigin, ObjMsgHost *fwd)
  {
    unordered_map<int, list<ObjMsgHost *>>::iterator found = forwards.find(fromOrigin);
    if (found != forwards.end())
    {
      found->second.push_back(fwd);
    }
    else
    {
      list<ObjMsgHost *> fwds;
      fwds.push_back(fwd);
      forwards[fromOrigin] = fwds;
    }
    return true;
  }

  /// Forward 'data' to consumers of its origin (if they are not the origin)
  /// @param data: data to forward
  void Forward(ObjMsgData *data)
  {
    unordered_map<int, list<ObjMsgHost *>>::iterator found = forwards.find(data->GetOrigin());
    if (found != forwards.end())
    {
      for (list<ObjMsgHost *>::iterator fit = found->second.begin(); fit != found->second.end(); ++fit)
      {
        if (!data->IsFrom((*fit)->GetOrigin()))
        {
          (*fit)->Consume(data);
        }
        else {
        }
      }
    }
  }

  /// Wait on receive queue for the next message, forward as configured and deliver to caller
  ///
  /// @param dataRef: Caller's reference to receive message
  /// @param xTicksToWait: Timeout on wait
  /// @return boolean success
  bool Receive(ObjMsgDataRef &dataRef, TickType_t xTicksToWait)
  {
    ObjMessage *msg;
    bool result = xQueueReceive(message_queue, &msg, xTicksToWait) ? true:false;
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
  unordered_map<int, list<ObjMsgHost *>> forwards;
};
