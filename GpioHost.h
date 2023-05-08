#pragma once

#include "driver/gpio.h"
// #include <algorithm>

#include "ObjMsg.h"

enum GpioFlags // GPIO flags (_GF)
{
  INVERTED_GF = 0x01,
  POS_EVENT_GF = 0x04, // POS / NEG edge when inverted applied
  NEG_EVENT_GF = 0x08,
  IS_INPUT_GF = 0x10,
  PULLDOWN_GF = 0x20,
  PULLUP_GF = 0x20,
};
inline GpioFlags operator|(GpioFlags a, GpioFlags b)
{
    return static_cast<GpioFlags>(static_cast<int>(a) | static_cast<int>(b));
}

typedef ObjMsgDataInt ObjMsgGpioData;
class GpioHost;

class GpioPort
{
public:
  /** Default constructor to support map */
  GpioPort() {}
  /** Add 'channel' with range from zero to 'maxCount',
   * and scaling from 'min' to 'max'
   */
  GpioPort(string name, gpio_num_t pin, ObjMsgSample mode, GpioFlags flags, GpioHost *host)
  {
    this->host = host;
    this->name = name;
    this->mode = mode;
    this->pin = pin;
    this->flags = flags;
    changed = 0;
  }

  int GetValue()
  {
    return value;
  }
  GpioHost *host;
  string name;
  gpio_num_t pin;
  ObjMsgSample mode;
  GpioFlags flags;
  int8_t changed; // +1, 0, -1 == on, none, off

  uint8_t value; // Measured value
};

class GpioHost : public ObjMsgHost
{
public:
  unordered_map<string, GpioPort> ports;
  bool anyChangeEvents;
  QueueHandle_t event_queue;

  GpioHost(ObjMsgTransport &transport, uint16_t origin)
      : ObjMsgHost(transport, "GpioHost", origin)
  {
    event_queue = NULL;
  }

  GpioPort *Add(string name, ObjMsgSample mode, GpioFlags flags, gpio_num_t pin)
  {
    ports[name] = GpioPort(name, pin, mode, flags, this);
    GpioPort *tmp = &ports[name];

    if (mode == CHANGE_EVENT)
    {
      // TODO make sure that IS_INPUT and at least one edge is also specified
      anyChangeEvents = true;
    }
    dataFactory.registerClass(origin_id, name, ObjMsgGpioData::create);

    gpio_config_t io_conf;

    if (flags & IS_INPUT_GF) // configure GPIO with the given settings
    { 
      // INPUT
      io_conf.mode = GPIO_MODE_INPUT;               // set as input mode
      io_conf.intr_type = EdgeConfig(flags);        // interrupt of both edges
      gpio_config(&io_conf);
    }
    else
    {
      // OUTPUT
      io_conf.mode = GPIO_MODE_OUTPUT;              // set as output mode
      io_conf.intr_type = GPIO_INTR_DISABLE;        // disable interrupt
    }
    io_conf.pin_bit_mask = 1ull << pin;           // bit mask of the pins, use GPIO4/5 here
    io_conf.pull_down_en = (flags & PULLDOWN_GF) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = (flags & PULLUP_GF) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    return tmp;
  }

  bool consume(ObjMsgData *data)
  {
    // consider NOT IS_INPUT_GF
    return false;
  }

  bool start()
  {
    if (anyChangeEvents)
    {
      unordered_map<string, GpioPort>::iterator it;

      event_queue = xQueueCreate(10, sizeof(void *));
      xTaskCreate(input_task, "gpio_input_task",
                  CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE + 50, this, 10, NULL);

      // install gpio isr service
      gpio_install_isr_service(0);

      for (it = ports.begin(); it != ports.end(); it++)
      {
        GpioPort &port = it->second;
        if (port.mode == CHANGE_EVENT)
        {
          gpio_isr_handler_add(port.pin, isr_handler, &port);
        }
      }
    }
    return true;
  }

  int Measure(string name)
  {
    GpioPort *port = GetPort(name);
    if (port)
    {
      return Measure(port);
    }
    return INT_MIN;
  }

  bool Measure(GpioPort *port)
  {
    // consider IS_INPUT_GF
    int level = gpio_get_level(port->pin);
    uint8_t flags = port->flags;
    if (flags & INVERTED_GF)
    {
      level = !level;
    }
    if (level != port->value)
    {
      port->changed = level > port->value ? 1 : -1;
      port->value = level;
    }
    else
    {
      port->changed = 0;
    }
    return port->value;
  }

protected:
  GpioPort *GetPort(string name)
  {
    unordered_map<string, GpioPort>::iterator found = ports.find(name);
    if (found != ports.end())
    {
      return &found->second;
    }
    else
    {
      return NULL;
    }
  }
  static void input_task(void *arg)
  {
    GpioHost *host = (GpioHost *)arg;
    for (;;)
    {
      GpioPort *port = NULL;
      if (xQueueReceive(host->event_queue, &port, portMAX_DELAY))
      {
        host->Measure(port);

        if (((port->changed > 0) && (port->flags & POS_EVENT_GF)) || ((port->changed < 0) && (port->flags & NEG_EVENT_GF)))
        {
          ObjMsgDataRef point = ObjMsgGpioData::create(host->origin_id, port->name.c_str(), port->value);
          host->produce(point);
        }
      }
    }
  }

  static void isr_handler(void *arg)
  {
    GpioPort *port = (GpioPort *)arg;
    xQueueSendFromISR(port->host->event_queue, &port, NULL);
  }

private:
  gpio_int_type_t EdgeConfig(int flags)
  {
    gpio_int_type_t edge = GPIO_INTR_DISABLE;
    if (flags & POS_EVENT_GF)
    {
      edge = (gpio_int_type_t)(edge | (flags & INVERTED_GF) ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE);
    }
    if (flags & NEG_EVENT_GF)
    {
      edge = (gpio_int_type_t)(edge | (flags & INVERTED_GF) ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE);
    }
    return edge;
  }
};