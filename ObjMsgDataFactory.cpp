#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "ObjMsg.h"

// Constructor for static datafactory in ObjMsgData
ObjMsgDataFactory ObjMsgData::dataFactory;

bool ObjMsgHost::Produce(ObjMsgDataRef data)
{
  return transport->Send(data);
}


/** register object creator function 'fn' to create object for endpoint 'name' */
bool ObjMsgDataFactory::RegisterClass(uint16_t origin, string name, ObjMsgDataRef (*fn)(uint16_t, char const *))
{
  return dataClasses.insert(make_pair(name, fn)).second;
}
/** Create ObjMsgDataRef object for endpoint 'name' */
ObjMsgDataRef ObjMsgDataFactory::Create(uint16_t origin, char const *name)
{
  ObjMsgDataRef (*fn)(uint16_t, char const *) = dataClasses[name];
  if (fn)
    return fn(origin, name);
  return NULL;
}
/** Create ObjMsgDataRef object and populate it based on 'json' content */
ObjMsgDataRef ObjMsgDataFactory::Deserialize(uint16_t origin, char const *json)
{
  ObjMsgDataRef data = NULL;

  cJSON *root = cJSON_Parse(json);
  if (root)
  {
    cJSON *jsonName = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (jsonName)
    {
      data = Create(origin, cJSON_GetStringValue(jsonName));
      if (data)
      {
        if (data.get()->DeserializeValue(root))
        {
          cJSON_Delete(root);
          return data;
        }
        else
        {
          ESP_LOGE("Deserialize", "Unable to parse value: %s", json);
        }
      }
      else
      {
        //printf("Create(%d, %s, %s)\n", origin, cJSON_GetStringValue(jsonName),
        //       cJSON_Print(cJSON_GetObjectItem(root, "value")));
        // If not registered, deliver as JSON object carried in string
        data = ObjMsgDataString::Create(origin, cJSON_GetStringValue(jsonName),
                                        cJSON_Print(cJSON_GetObjectItem(root, "value")), true);
        ESP_LOGI("Deserialize", "No class registered for: %s", cJSON_GetStringValue(jsonName));

        cJSON_Delete(root);
        return data;
      }
    }
    else
    {
      ESP_LOGE("Deserialize", "JSON has no name field: %s", json);
    }
    cJSON_Delete(root);
  }
  else
  {
    ESP_LOGE("Deserialize", "JSON malformed: %s", json);
  }

  return NULL;
}
