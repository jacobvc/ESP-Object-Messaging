#pragma once

#include "esp_adc/adc_oneshot.h"
#include <algorithm>

#include "ObjMsg.h"

typedef ObjMsgDataInt ObjMsgAdcData;

/// A channel hosted by AdcHost
class AdcChannel
{
public:
  /// Default constructor to support map 
  AdcChannel() {}

  /// Instantiate 'channel' with range from zero to 'maxCount',
  /// and scaling from 'min' to 'max'
  /// @param channel: ADC Channel
  /// @param mode: Sampling mode (manual vs event)
  /// @param maxCount: ADC channel configured max value
  /// @param min: Calculated value for ZERO count
  /// @param max: Calculated value for maxCount
  /// @param hysteresis: Minimum value change for new event
  AdcChannel(adc_channel_t channel, ObjMsgSample mode, int maxCount,
             int min, int max, int hysteresis = 5)
  {
    this->mode = mode;
    this->channel = channel;
    this->rMax = maxCount;
    // Set reference count to full scale (only one segment)
    this->rRef = maxCount;

    // set value range to double to support rounding
    this->vMinX2 = 2 * min;
    this->vMaxX2 = 2 * max;
    // Set reference value to max value (full range)
    this->vRefX2 = this->vMaxX2;

    this->hysteresis = hysteresis;
  }

  /// Set a known value reference point for the current sample.
  /// @param reference: Known value
  void SetReference(int reference)
  {
    vRefX2 = reference * 2;
    rRef = raw;
  }

  /// Calculate the scaled value for 'rawAdc' measurement.
  /// 
  /// Calculate the 2X value using the segments {0, vMinX2} to {rRef, vRefX2}
  /// and {rRef, vRefX2} to {rMax, vMaxX2},
  /// one of which may be zero length, then round to value
  /// @param rawAdc: The raw measurement
  /// @return calculated value
  int SetValue(int rawAdc)
  {
    // Cap at measurement range
    raw = std::min(std::max(rawAdc, 0), rMax);
    int valueX2;

    if (raw >= rRef)
    {
      // Second segment, from vRefX2 to vMaxX2 over rRef to rMax
      valueX2 = vRefX2 + (vMaxX2 - vRefX2) * (raw - rRef) / (rMax - rRef);
    }
    else
    {
      // First segment, from vMinX2 to vRefX2 over zero to rRef
      valueX2 = vMinX2 + (vRefX2 - vMinX2) * raw / rRef;
    }
    // Round result
    value = (valueX2 + ((valueX2 > 0) ? 1 : -1)) / 2;
    return value;
  }
  int GetValue()
  {
    return value;
  }
  // Underlying ESP32 channel
  adc_channel_t channel;
  ObjMsgSample mode;
  int hysteresis;

protected:
  // Raw variables
  int raw;  ///< Raw measurement
  int rMax; ///< Max raw count (min is zero)
  int rRef; ///< Raw count reference point
  // Value variables
  int vMinX2; ///< Min value
  int vMaxX2; ///< Max value
  int vRefX2; ///< Value at reference point
  int value;  ///< Measured value
};

/// ObjMsgHost, hosting ADC_UNIT_1
class AdcHost : public ObjMsgHost
{
public:
  /// Constructor, specifying transport object, origin, and samlpe interval
  /// @param transport: Transport object
  /// @param origin: Origin ID for this host
  /// @param sampleIntervalMs: sampling interval for event detection
  AdcHost(ObjMsgTransport &transport, uint16_t origin, 
    TickType_t sampleIntervalMs)
      : ObjMsgHost(transport, "AdcHost", origin)
  {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    anyChangeEvents = false;
    this->sampleIntervalMs = sampleIntervalMs;
  }

  /// Add 'channel' as 'name', configured with 'atten', 'bitwidth', and 'maxCounts'
  /// to operate in 'mode' and produce values from 'min' to 'max'.
  /// @param name: Data object name 
  /// @param mode: Sampling mode (manual vs event)
  /// @param channel: ADC Channel
  /// @param atten 
  /// @param bitwidth 
  /// @param maxCounts: ADC channel configured max value
  /// @param min: Calculated value for ZERO count
  /// @param max: Calculated value for maxCount
  /// @return added channel
  AdcChannel *Add(string name, ObjMsgSample mode, adc_channel_t channel, 
    adc_atten_t atten, adc_bitwidth_t bitwidth,
    int16_t maxCounts, int32_t min, int32_t max)
  {
    adc_oneshot_chan_cfg_t config = {
        .atten = atten,
        .bitwidth = bitwidth};

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &config));

    channels[name] = AdcChannel(channel, mode, maxCounts, min, max);
    AdcChannel *tmp = &channels[name];

    if (mode == CHANGE_EVENT)
    {
      anyChangeEvents = true;
    }

    ObjMsgData::RegisterClass(origin_id, name, ObjMsgAdcData::Create);

    return tmp;
  }

  /// Measure channel named 'name' 
  /// @param name: Name of channel to measure
  /// @return measured value, or INT_MIN if 'name' is not an AdcChannel
  int Measure(string name)
  {
    AdcChannel *channel = GetChannel(name);
    if (channel)
    {
      return Measure(channel);
    }
    return INT_MIN;
  }

  /// Measure 'channel'
  /// @param channel: Channel to measure
  /// @return measured value
  int Measure(AdcChannel *channel)
  {
    int rawValue;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel->channel, &rawValue));
    // Set value, applying scaling
    return channel->SetValue(rawValue);
  }

  bool Start()
  {
    if (anyChangeEvents)
    {
      xTaskCreate(task, "adc_task", CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE, 
        this, 10, NULL);
    }
    return true;
  }

protected:
  AdcChannel *GetChannel(string name)
  {
    unordered_map<string, AdcChannel>::iterator found = channels.find(name);
    if (found != channels.end())
    {
      return &found->second;
    }
    else
    {
      return NULL;
    }
  }

  static void task(void *arg)
  {
    AdcHost *ep = (AdcHost *)arg;

    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(ep->sampleIntervalMs));
      unordered_map<string, AdcChannel>::iterator it;

      for (it = ep->channels.begin(); it != ep->channels.end(); it++)
      {
        AdcChannel &js = it->second;
        if (js.mode == CHANGE_EVENT)
        {
          int value = js.GetValue();
          ep->Measure(&js);
          if (abs(value - js.GetValue()) > js.hysteresis)
          {
            ObjMsgDataRef data = ObjMsgAdcData::Create(
              ep->origin_id, it->first.c_str(), js.GetValue());
            ep->Produce(data);
          }
        }
      }
    }
  }
  
  TickType_t sampleIntervalMs;
  adc_oneshot_unit_handle_t adc_handle;
  unordered_map<string, AdcChannel> channels;
  bool anyChangeEvents;
};
