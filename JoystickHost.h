/** @file
 *
 * The joystick class initializes and provides for operation of joysticks.
 *
 */
#pragma once

#include "driver/gpio.h"
#include <unordered_map>

#include "ObjMsg.h"
#include "ObjMsgJoystickData.h"
#include "AdcHost.h"

// Sample count to average
#define NO_OF_SAMPLES 5

class JoystickHost : public ObjMsgHost
{
public:
  JoystickHost(AdcHost &adc, ObjMsgTransport &transport, uint16_t origin, 
    TickType_t sampleIntervalMs)
      : ObjMsgHost(transport, "JOYSTICK", origin), adc(adc)
  {
    this->sampleIntervalMs = sampleIntervalMs;
    anyChangeEvents = false;
  }

  int add(string name, ObjMsgSample mode, 
    adc_channel_t ad_x, adc_channel_t ad_y, gpio_num_t btn)
  {
    joysticks[name] = Joystick(name, btn, mode);
    Joystick &joy = joysticks[name];

    if (mode == CHANGE_EVENT) {
      anyChangeEvents = true;
    }

    AdcChannel *ch = adc.Add(name + "-x", POLLING, ad_x, 
      ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, -100, 100);
    joy.xchan = ch;
    ch = adc.Add(name + "-y", POLLING, ad_y, 
      ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, -100, 100);
    joy.ychan = ch;

    dataFactory.registerClass(origin_id, name, ObjMsgJoystickData::create);

    init_switch(btn);

    return 0;
  }

  bool start()
  {
    if (anyChangeEvents) {
      xTaskCreate(task, "joystick_task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE, 
        this, 10, NULL);
    }

    return true;
  }
  bool center(string name, int x, int y)
  {
    Joystick *js = GetJoystick(name);
    if (js)
    {
      js->xchan->SetReference(x);
      js->ychan->SetReference(y);
      return true;
    }
    else
    {
      return false;
    }
  }

protected:
  class Joystick
  {
  public:
    Joystick() {}
    Joystick(string name, gpio_num_t btn, ObjMsgSample mode, 
      int hysteresis = 5)
    {
      this->name = name;
      this->mode = mode;
      this->btnpin = btn;
      this->centered = false;
      this->changed = 0;
      this->hysteresis = hysteresis;
      memset(&sample, 0, sizeof(sample));
    }
    string name;
    // Configuration
    AdcChannel *xchan;
    AdcChannel *ychan;
    gpio_num_t btnpin;
    // Calibration
    bool centered;

    ObjMsgSample mode;
    int hysteresis;
    int16_t changed;
    joystick_sample_t sample;
  };

  bool anyChangeEvents;
  unordered_map<string, Joystick> joysticks;
  TickType_t sampleIntervalMs;
  AdcHost &adc;

  Joystick *GetJoystick(string name)
  {
    unordered_map<string, Joystick>::iterator found = joysticks.find(name);
    if (found != joysticks.end())
    {
      return &found->second;
    }
    else
    {
      return NULL;
    }
  }

  void init_switch(int btn)
  {
    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // bit mask of the pins
    io_conf.pin_bit_mask = 1ull << btn;
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // Enable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    // gpio_set_drive_capability(btn, GPIO_DRIVE_CAP_3 );
  }

  static void task(void *arg)
  {
    JoystickHost *ep = (JoystickHost *)arg;

    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(ep->sampleIntervalMs));
      unordered_map<string, Joystick>::iterator it;

      for (it = ep->joysticks.begin(); it != ep->joysticks.end(); it++)
      {
        Joystick &js = it->second;
        if (js.mode == CHANGE_EVENT)
        {
          if (ep->measure(&js))
          {
            ObjMsgDataRef data = ObjMsgJoystickData::create(
              ep->origin_id, js.name.c_str(), js.sample);
            ep->produce(data);
          }
        }
      }
    }
  }

  int measure(Joystick *js)
  {
    int32_t reading_x = 0;
    int32_t reading_y = 0;
    int16_t up;
    joystick_sample_t *sample = &js->sample;

    // Measure NO_OF_SAMPLES times and average the reading
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
      reading_x += adc.Read(js->xchan);
      reading_y += adc.Read(js->ychan);
    }
    up = gpio_get_level(js->btnpin);

    reading_x /= NO_OF_SAMPLES;
    reading_y /= NO_OF_SAMPLES;

    // Use first sample value for initial center position
    if (!js->centered)
    {
      js->centered = true;
      js->xchan->SetReference(0);
      js->ychan->SetReference(0);
    }

    if (abs(sample->x - reading_x) > js->hysteresis 
      || abs(sample->y - reading_y) > js->hysteresis || sample->up != up)
    {
      sample->x = reading_x;
      sample->y = reading_y;
      sample->up = up;
      js->changed = true;

      return 1;
    }
    else
    {
      // No change
      js->changed = false;
      return 0;
    }
  }
};
