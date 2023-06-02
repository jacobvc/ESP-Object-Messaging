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
#include "GpioHost.h"

// Sample count to average
#define NO_OF_SAMPLES 5

/// ObjMsgHost, hosting joysticks
class JoystickHost : public ObjMsgHost
{
public:
  /// Constructor, specifying transport object and origin 
  /// @param adc: AdcHost, hosting analog inputs
  /// @param gpio: GpioHost, hosting digital inouts
  /// @param transport: Transport object
  /// @param origin: Origin ID for this host
  /// @param sampleIntervalMs: sampling interval for event detection
  JoystickHost(AdcHost &adc, GpioHost &gpio, ObjMsgTransport &transport, uint16_t origin, 
    TickType_t sampleIntervalMs)
      : ObjMsgHost(transport, "JOYSTICK", origin), adc(adc), gpio(gpio)
  {
    this->sampleIntervalMs = sampleIntervalMs;
    anyChangeEvents = false;
  }

  /// Add joystick as 'name', to operate in 'mode' with analog channels 'ad_x' and
  /// 'ad_y', and switch GPIO input 'btn'
  /// @param name: Data object name 
  /// @param mode: Sampling mode (manual vs event)
  /// @param ad_x: ADC channel for x input 
  /// @param ad_y: ADC channel for y input 
  /// @param btn: GPIO port for button
  /// @return 
  int Add(string name, ObjMsgSample mode, 
    adc_channel_t ad_x, adc_channel_t ad_y, gpio_num_t btn)
  {
    joysticks[name] = new Joystick(name, mode);
    Joystick *joy = joysticks[name];

    if (mode == CHANGE_EVENT) {
      anyChangeEvents = true;
    }

    AdcChannel *ch = adc.Add(name + "-x", POLLING, ad_x, 
      ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, -100, 100);
    joy->xchan = ch;
    ch = adc.Add(name + "-y", POLLING, ad_y, 
      ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, -100, 100);
    joy->ychan = ch;

    joy->btnPort = gpio.Add(name + "-up", btn, POLLING, IS_INPUT_GF | PULLUP_GF);

    ObjMsgData::RegisterClass(origin_id, name, ObjMsgJoystickData::Create);

    return 0;
  }

  bool Start()
  {
    if (anyChangeEvents) {
      xTaskCreate(Task, "joystick_task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE, 
        this, 10, NULL);
    }

    return true;
  }

  bool Center(string name, int x, int y)
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
    Joystick(string name, ObjMsgSample mode, 
      int hysteresis = 5)
    {
      this->name = name;
      this->mode = mode;
      this->centered = false;
      this->changed = 0;
      this->hysteresis = hysteresis;
      memset(&sample, 0, sizeof(sample));
    }
    string name;
    // Configuration
    AdcChannel *xchan;
    AdcChannel *ychan;
    GpioPort *btnPort;
    // Calibration
    bool centered;

    ObjMsgSample mode;
    int hysteresis;
    int16_t changed;
    joystick_sample_t sample;
  };

  bool anyChangeEvents;
  unordered_map<string, Joystick *> joysticks;
  TickType_t sampleIntervalMs;
  AdcHost &adc;
  GpioHost &gpio;

  Joystick *GetJoystick(string name)
  {
    unordered_map<string, Joystick *>::iterator found = joysticks.find(name);
    if (found != joysticks.end())
    {
      return found->second;
    }
    else
    {
      return NULL;
    }
  }

  static void Task(void *arg)
  {
    JoystickHost *ep = (JoystickHost *)arg;

    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(ep->sampleIntervalMs));
      unordered_map<string, Joystick *>::iterator it;

      for (it = ep->joysticks.begin(); it != ep->joysticks.end(); it++)
      {
        Joystick *js = it->second;
        if (js->mode == CHANGE_EVENT)
        {
          if (ep->Measure(js))
          {
            ObjMsgDataRef data = ObjMsgJoystickData::Create(
              ep->origin_id, js->name.c_str(), js->sample);
            ep->Produce(data);
          }
        }
      }
    }
  }

  bool Measure(Joystick *js)
  {
    int32_t reading_x = 0;
    int32_t reading_y = 0;
    int16_t up;
    joystick_sample_t *sample = &js->sample;

    // Measure NO_OF_SAMPLES times and average the reading
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
      reading_x += adc.Measure(js->xchan);
      reading_y += adc.Measure(js->ychan);
    }
    up = gpio.Measure(js->btnPort);

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

      return true;
    }
    else
    {
      // No change
      js->changed = false;
      return false;
    }
  }
};
