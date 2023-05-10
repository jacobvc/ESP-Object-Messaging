# ESP-Object-Messaging
Object oriented messaging for ESP32 with JSON encoding

Support for relativly simple integration of components into an application 
that operates based on exchanging messages containing C++ objects.

The content is conveyed as ObjMsgDataRef, a std:shared_ptr to a ObjMsgData
derived class. ObjMsgData classes are generally implemented using the
templatized ObjMsgDataT class.

The transport mechanism is a ObjMsgTransport object which supports Send() and Receive().

Information is produced and transported using ObjMsgHost::Produce(), received (at a single point) using ObjMsgTransport::Receive(), and distributed to desired endpoints using ObjMsgHost::Consume().

## ObjMsgData
All data is exchanged using objects derived from ObjMsgData base class

Encodes origin, endpoint name, and binary data with JSON serialization
and deserialization.

## ObjMsgDataT
Abstract base class for templatized ObjMsgDataT. Intended to always be
instantiated using a ObjMsgDataRef (a std::shared_ptr)

## ObjMsgDataRef
A std::shared_ptr encoding ObjMsgData.

## ObjMsgTransport
ObjMsgTransport intantiates a freertos 'message_queue' which exchanges
ObjMessage's, carrying ObjMsgData.

## ObjMsgHost
ObjMsgHost implements produce() which attaches ObjMsgData to a new ObjMessage and
sends it using ObjMsgTransport.

The application receives produced ObjMsgData by calling transport.receive()
which waits up to a specified time for a message, deletes the message (after
removing the attached data), and invokes consume() on intended destinations.

Each library component may produce content, and sends it by invoking 
this->produce() identifying itself as origin and passing the produced
content as a name / value pair.

 Each component may also consume content, and  must support that by implementing consume() which is typically called by the application 
 (controller) to deliver ObjMsgData.

## ObjMsgDataFactory
 The ObjMsgDataFactory supports creation of a ObjMsgData object based on recevied data.
 The initial implementation supports only JSON data. Each endpoint supporting
 object creation must register with the factory, this example (for data of
 type ObjMsgDataInt32):

    dataFactory.registerClass(my_origin, "my_name", ObjMsgDataInt32::create);

 Will create a ObjMsgDataInt32 object when data.get() is called for
 JSON data containing "name":"my_name".
