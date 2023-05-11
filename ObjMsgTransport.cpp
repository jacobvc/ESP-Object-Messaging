#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "ObjMsg.h"

ObjMsgDataFactory dataFactory;

BaseType_t ObjMsgTransport::Send(ObjMsgDataRef data)
{
  // Create message object to send shared_ptr data
  ObjMessage *msg = new ObjMessage(data);
  // and send it
  return xQueueSend(message_queue, &msg, 0);
}

BaseType_t ObjMsgTransport::Receive(ObjMsgDataRef& data, TickType_t xTicksToWait)
{
  ObjMessage *msg;
  BaseType_t result = xQueueReceive(message_queue, &msg, xTicksToWait);
  if (result) {
    // Received a message. Give caller reference to its data and delete the shared_ptr message object
    data = msg->data();
    delete msg;
  }
  // Return message reception result
  return result;
}

ObjMsgTransport::ObjMsgTransport(uint16_t message_queue_depth)
{
    message_queue = xQueueCreate(message_queue_depth, sizeof(ObjMessage*));
}