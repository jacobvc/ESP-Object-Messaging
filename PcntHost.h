#pragma once

#include "driver/pulse_cnt.h"

#include "ObjMsg.h"

typedef ObjMsgDataInt ObjMsgPcntData;
class PcntHost;

enum encoderType {
  SINGLE_ET,
  HALF_QUAD_ET,
  QUAD_ET
};

/// A pulse count unit hosted by PcntHost
class PcntUnit
{
public:
  /// Default constructor to support map
  PcntUnit() {}

  /// Instantiate  'pin' on 'host' as 'name' operating in 'mode' and confgured using 'flags
  /// @param name: Data object name
  /// @param mode: Sampling mode (manual vs event)
  /// @param host: host of this unit
  PcntUnit(string name, ObjMsgSample mode, PcntHost* host)
  {
    this->name = name;
    this->mode = mode;
    this->host = host;

    changed = 0;
  }

  /// Value accessor
  /// @return value
  int GetValue()
  {
    return value;
  }
  string name;
  ObjMsgSample mode;
  PcntHost* host;

  pcnt_unit_handle_t pcnt;

  int8_t changed; // value {+1, 0, -1} == changed to {on, none, off}
  int value; // Measured value
};

/// An ObjMsgHost object, hosting Pulse Count units
class PcntHost : public ObjMsgHost
{
public:
  unordered_map<string, PcntUnit*> units;
  bool anyChangeEvents;
  QueueHandle_t event_queue;
  TickType_t sampleIntervalMs;

  /// Constructor, specifying transport object and origin
  /// @param transport: Transport object
  /// @param origin: Origin ID for this host
  PcntHost(ObjMsgTransport* transport, uint16_t origin, TickType_t sampleIntervalMs)
    : ObjMsgHost(transport, "PcntHost", origin), sampleIntervalMs(sampleIntervalMs)
  {
    event_queue = NULL;
  }

  /// Add 'pin' as 'name', to operate in 'mode' and configured by 'flags'
  /// @param name: Data object name
  /// @param inputA: GPIO 
  /// @param inputB: GPIO 
  /// @param et: Encoder type (single, half-quad, quad)
  /// @param mode: Sampling mode (manual vs event)
  /// @param lowLimit: Rollover value
  /// @param highLimit: Rollover value
  /// @return created PcntUnit
  PcntUnit* Add(string name, gpio_num_t inputA, gpio_num_t inputB, encoderType et,
    ObjMsgSample mode, int lowLimit = INT16_MIN, int highLimit = INT16_MAX)
  {
    units[name] = new PcntUnit(name, mode, this);
    PcntUnit* tmp = units[name];

    if (mode == CHANGE_EVENT) {
      anyChangeEvents = true;
    }
    ObjMsgPcntData::RegisterClass(origin_id, name, ObjMsgPcntData::Create);

    ESP_LOGI(TAG.c_str(), "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .low_limit = lowLimit,
        .high_limit = highLimit,
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &tmp->pcnt));

    ESP_LOGI(TAG.c_str(), "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 100,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(tmp->pcnt, &filter_config));

    ESP_LOGI(TAG.c_str(), "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = inputA,
        .level_gpio_num = inputB,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(tmp->pcnt, &chan_a_config, &pcnt_chan_a));

    ESP_LOGI(TAG.c_str(), "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a,
      et != SINGLE_ET ? PCNT_CHANNEL_EDGE_ACTION_DECREASE : PCNT_CHANNEL_EDGE_ACTION_HOLD,
      PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a,
      PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

      // Quad counting needs second channel
      pcnt_chan_config_t chan_b_config = {
          .edge_gpio_num = inputB,
          .level_gpio_num = inputA,
      };
      pcnt_channel_handle_t pcnt_chan_b = NULL;
      ESP_ERROR_CHECK(pcnt_new_channel(tmp->pcnt, &chan_b_config, &pcnt_chan_b));

    if (et == QUAD_ET) {
      ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
      ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    }
    else {
      ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b,
        PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_HOLD));
      ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_HOLD, PCNT_CHANNEL_LEVEL_ACTION_HOLD));
    }
    /*
        pcnt_event_callbacks_t cbs = {
            .on_reach = example_pcnt_on_reach,
        };
        QueueHandle_t queue = xQueueCreate(10, sizeof(int));
        ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(tmp->pcnt, &cbs, queue));
    */
    return tmp;
  }

  bool Start()
  {
    if (anyChangeEvents) {
      unordered_map<string, PcntUnit*>::iterator it;

      event_queue = xQueueCreate(10, sizeof(void*));
      xTaskCreate(input_task, "pulse_counter_input_task",
        CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE + 100, this, 10, NULL);

      for (it = units.begin(); it != units.end(); it++) {
        PcntUnit* unit = it->second;
        if (unit->mode == CHANGE_EVENT) {
          ESP_LOGI(TAG.c_str(), "enable pcnt unit");
          ESP_ERROR_CHECK(pcnt_unit_enable(unit->pcnt));
          ESP_LOGI(TAG.c_str(), "clear pcnt unit");
          ESP_ERROR_CHECK(pcnt_unit_clear_count(unit->pcnt));
          ESP_LOGI(TAG.c_str(), "start pcnt unit");
          ESP_ERROR_CHECK(pcnt_unit_start(unit->pcnt));
        }
      }
    }
    return true;
  }

  /// Measure unit named 'name'
  /// @param name: Name of unit to measure
  /// @return measured value, or INT_MIN if 'name' is not a PcntUnit
  int Measure(string name)
  {
    PcntUnit* unit = GetUnit(name);
    if (unit) {
      return Measure(unit);
    }
    return INT_MIN;
  }

  /// Measure 'unit'
  /// @param channel: PcntUnit to measure
  /// @return measured value
  int Measure(PcntUnit* unit)
  {
    int pulse_count = 0;
    ESP_ERROR_CHECK(pcnt_unit_get_count(unit->pcnt, &pulse_count));

    if (pulse_count != unit->value) {
      unit->changed = pulse_count > unit->value ? 1 : -1;
      unit->value = pulse_count;
      //ESP_LOGI(TAG.c_str(), "Pulse count: %d", pulse_count);
    }
    else {
      unit->changed = 0;
    }
    return unit->value;
  }

protected:
  PcntUnit* GetUnit(string name)
  {
    unordered_map<string, PcntUnit*>::iterator found = units.find(name);
    if (found != units.end()) {
      return found->second;
    }
    else {
      return NULL;
    }
  }
  static void input_task(void* arg)
  {
    unordered_map<string, PcntUnit*>::iterator it;
    PcntHost* host = (PcntHost*)arg;

    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(host->sampleIntervalMs));
      for (it = host->units.begin(); it != host->units.end(); it++) {
        PcntUnit* unit = it->second;
        if (unit->mode == CHANGE_EVENT) {
          if (host->Measure(unit)) {
            if (unit->changed) {
              ObjMsgDataRef point = ObjMsgPcntData::Create(host->origin_id, unit->name.c_str(), unit->value);
              host->Produce(point);
            }
          }
        }
      }
    }
  }

private:
};