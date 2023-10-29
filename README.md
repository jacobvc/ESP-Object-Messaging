# ESP-Object-Messaging
Object oriented messaging for ESP32 with JSON encoding

Support for relatively simple integration of components into an application 
that operates based on exchanging messages conveying C++ objects.

The content is conveyed as ObjMsgDataRef, a std:shared_ptr to a ObjMsgData
derived class. ObjMsgData classes are generally implemented using the
templatized ObjMsgDataT class.

The transport mechanism is a ObjMsgTransport object which supports Send() and Receive().

Information is produced and transported by a resource host using 
ObjMsgHost Produce(), received (at a single point) using 
ObjMsgTransport Receive(), and distributed to desired endpoints using ObjMsgHost Consume().

# ObjMsg-Classes

## ObjMsgData
All data is exchanged using objects derived from ObjMsgData base class

Encodes origin, endpoint name, and binary data. Implements JSON serialization
and deserialization.

Implements GetValue() for string, integer, and double values, that return
false if that value type is not available.

## ObjMsgDataT
Abstract base class for templatized ObjMsgData. Intended to always be
instantiated using a ObjMsgDataRef (a std::shared_ptr)

Each ObjMsgDataT is expected to implement static Create() methods to instantiate
it (with or without data content) within an ObjMsgDataRef

Implements GetRawValue() to access the underlying binary value.

### ObjMsgDataT Implementations
Base ObjMsgDataT implementations include ObjMsgDataInt, ObjMsgDataFloat, and ObjMsgDataString

A virtual ObjMsgDataT, ObjMsgJoystickData, is included

## ObjMsgDataRef
A std::shared_ptr encoding ObjMsgData.

## ObjMsgTransport
ObjMsgTransport intantiates a freertos 'message_queue' which sends and receives
messages, carrying data as ObjMsgDataRef.

Send() creates a new message containing the provided 'data' and enqueues it to be sent

Receive() releases data from the next enqueued message to the caller,
then delete the message itself

## ObjMsgHost
ObjMsgHost implements Produce() which attaches ObjMsgData to a new ObjMessage and
sends it using ObjMsgTransport.

Each library host may produce content, and sends it by invoking 
this->Produce(), identifying itself as origin and passing the produced
content as a ObjMsgDataRef.

 Each host may also consume content, and  must support that by implementing Consume() which is typically called by the application 
 (controller) to deliver ObjMsgData.

### ObjMsgHost Implementations
Example implementations include AdcHost, GpioHost, ServoHost, WebsocketHost, LvglHost, JoystickHost 

## ObjMsgDataFactory
 The ObjMsgDataFactory supports creation of a ObjMsgData object based on received data. Each endpoint supporting
 object creation must register with the factory, this example (for data of
 type ObjMsgDataInt32):
```
    dataFactory.RegisterClass(my_origin, "my_name", ObjMsgDataInt32::Create);
```
 Will create a ObjMsgDataInt32 object when data.get() is called for
 JSON data containing "name":"my_name".
