/** @file
 *
 * The 3 channel joystick data
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
  int16_t z; ///< Z value
  int16_t up;///< Button position
} Joystick3AxisSample_t;

// ObjMsgDataT<Joystick3AxisSample_t>,  with joystick data structure value
class Joystick3AxisData : public ObjMsgDataT<Joystick3AxisSample_t>
{
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  Joystick3AxisData(uint16_t origin, char const *name, Joystick3AxisSample_t value)
      : ObjMsgDataT<Joystick3AxisSample_t>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(TAG.c_str(), "Joystick3AxisData(%u, %s, %u) constructed", origin, name, value);
  }
  /// Constructor without initial value
  /// @param origin: Data origin
  /// @param name: Data object name
  Joystick3AxisData(uint16_t origin, char const *name)
      : ObjMsgDataT<Joystick3AxisSample_t>(origin, name) {}

  /// Destructor
  ~Joystick3AxisData()
  {
    // ESP_LOGI(TAG.c_str(), "Joystick3AxisData destructed");
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const *name, Joystick3AxisSample_t value)
  {
    return std::make_shared<Joystick3AxisData>(origin, name, value);
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const *name)
  {
    return std::make_shared<Joystick3AxisData>(origin, name);
  }

  bool DeserializeValue(cJSON *json)
  {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "value");
    value.x = cJSON_GetObjectItem(item, "x")->valueint;
    value.y = cJSON_GetObjectItem(item, "y")->valueint;
    value.z = cJSON_GetObjectItem(item, "z")->valueint;
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
    sprintf(buffer, " { \"x\":%d, \"y\":%d, \"z\":%d, \"up\":%d }",
            value.x, value.y, value.z, value.up);
    str = buffer;

    return true;
  }
};
