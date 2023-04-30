/** @file
 *
 * The ObjMsgJoystickData class is an example of a non-scalar 'ObjMsgDataT'
 *
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include "ObjMsgData.h"

typedef struct
{
  int16_t x;
  int16_t y;
  int16_t up;
} joystick_sample_t;

/** ObjMsgData with joystick data structure value  */
class ObjMsgJoystickData : public ObjMsgDataT<joystick_sample_t>
{
public:
  ObjMsgJoystickData(uint16_t origin, char const *name, joystick_sample_t value)
      : ObjMsgDataT<joystick_sample_t>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgJoystickData(%u, %s, %u) constructed", origin, name, value);
  }
  ObjMsgJoystickData(uint16_t origin, char const *name)
      : ObjMsgDataT<joystick_sample_t>(origin, name) {}

  ~ObjMsgJoystickData()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgJoystickData destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name, joystick_sample_t value)
  {
    return std::make_shared<ObjMsgJoystickData>(origin, name, value);
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgJoystickData>(origin, name);
  }

  bool deserializeValue(cJSON *json)
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
