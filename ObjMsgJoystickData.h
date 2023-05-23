/** @file
 *
 * The ObjMsgJoystickData class is an example of a non-scalar 'ObjMsgDataT'
 *
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include "ObjMsgData.h"

/// Joystick sample data
typedef struct
{
  int16_t x; ///< X value
  int16_t y; ///< Y value
  int16_t up;///< Button position
} joystick_sample_t;

// ObjMsgDataT<joystick_sample_t>,  with joystick data structure value
class ObjMsgJoystickData : public ObjMsgDataT<joystick_sample_t>
{
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  ObjMsgJoystickData(uint16_t origin, char const *name, joystick_sample_t value)
      : ObjMsgDataT<joystick_sample_t>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(TAG.c_str(), "ObjMsgJoystickData(%u, %s, %u) constructed", origin, name, value);
  }
  /// Constructor without initial value
  /// @param origin: Data origin
  /// @param name: Data object name
  ObjMsgJoystickData(uint16_t origin, char const *name)
      : ObjMsgDataT<joystick_sample_t>(origin, name) {}

  /// Destructor
  ~ObjMsgJoystickData()
  {
    // ESP_LOGI(TAG.c_str(), "ObjMsgJoystickData destructed");
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const *name, joystick_sample_t value)
  {
    return std::make_shared<ObjMsgJoystickData>(origin, name, value);
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgJoystickData>(origin, name);
  }

  bool DeserializeValue(cJSON *json)
  {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "value");
    value.x = cJSON_GetObjectItem(item, "x")->valueint;
    value.y = cJSON_GetObjectItem(item, "y")->valueint;
    value.up = cJSON_GetObjectItem(item, "up")->valueint;
    return true;
  }

  bool GetValue(int &val)
  {
    // val = value;
    return false;
  }
  bool GetValue(double &val)
  {
    // val = value;
    return false;
  }
  bool GetValue(string &str)
  {
    char buffer[80];
    sprintf(buffer, " { \"x\":%d, \"y\":%d, \"up\":%d }",
            value.x, value.y, value.up);
    str = buffer;

    return true;
  }
};
