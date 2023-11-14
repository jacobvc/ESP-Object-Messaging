/** @file
 *
 * The LvglHost class provides messaging with a
 * LVGL display, especially optimized to work with 
 * Squareline designed displays
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include <LvglBinding.h>
#include <unordered_map>
#include "ObjMsg.h"
#include "stdio.h"
// typedef ObjMsgDataInt ObjMsgServoData;
typedef bool (*lvglVirtualComsumer)(LvglHost *host, ObjMsgData *data);

class LvglHost : public ObjMsgHost
{
  typedef struct
  {
    const char *name;
    lv_obj_t *obj;
    enum ControlType type;
    lv_event_code_t eventCode;
  } control_reg_def_t;

public:
  /// Constructor, specifying transport object and origin 
  /// @param transport: Transport object
  /// @param origin: Origin ID for this host
  LvglHost(ObjMsgTransport *transport, uint16_t origin)
      : ObjMsgHost(transport, "LVGL", origin)
  {
  }

  /// add function 'consumer' as virtual consumer for data 'name' 
  /// @param name: Data object name 
  /// @param consumer: virtual consumer function
  /// @return boolean success
  int AddVirtualConsumer(const char *name, lvglVirtualComsumer consumer)
  {
    virtual_consume_map[name] = consumer;

    return true;
  }

  /// Add 'control' of type 'binding' as consumer of data 'name' 
  /// @param name: Data object name 
  /// @param control: LVGL object pointer
  /// @param  binding: ControlType 
  /// @return boolean success
  int AddConsumer(const char *name, lv_obj_t *control, enum ControlType binding)
  {
    // Note: eventCode member of control_reg_def_t is NOT used for a counsumer
    control_reg_def_t def = {.name = name, .obj = control, .type = binding, .eventCode = LV_EVENT_ALL};
    consume_map[name] = def;

    return true;
  }
  /// Add 'control' of type 'binding' as producer of data 'name' upon 'eventCode' 
  /// @param name: Data object name 
  /// @param control: LVGL object pointer
  /// @param  binding: ControlType 
  /// @param eventCode: Event that triggers production
  /// @return boolean success
  int AddProducer(const char *name, lv_obj_t *control, enum ControlType binding, lv_event_code_t eventCode)
  {
    control_reg_def_t def = {.name = name, .obj = control, .type = binding, .eventCode = eventCode};
    produce_map[control] = def;

    // Store this object in user_data for lookup
    lv_obj_add_event_cb(control, produce_cb, eventCode, this);

    return true;
  }

  bool Start()
  {
    return true;
  }

  bool Produce(ObjMsgDataRef data)
  {
    return transport->Send(data);
  }

  bool Consume(ObjMsgData *msg)
  {
    string strVal;
    int intVal;
    lvglVirtualComsumer vrt = GetVirtualConsumer(msg->GetName());
    if (vrt)
    {
      return vrt(this, msg);
    }
    else
    {
      control_reg_def_t *ctx = GetConsumer(msg->GetName());
      if (ctx)
      {
        switch (ctx->type)
        {
        case ARC_CT:
          if (msg->GetValue(intVal))
          {
            lv_arc_set_value(ctx->obj, intVal);
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume name (%s) value must be integer", msg->GetName().c_str());
            return false;
          }
          break;
        case BUTTON_CT:
          if (msg->GetValue(intVal))
          {
            if (intVal)
            {
              lv_obj_add_state(ctx->obj, LV_STATE_PRESSED);
            }
            else
            {
              lv_obj_clear_state(ctx->obj, LV_STATE_PRESSED);
            }
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume (%s) button value must be integer", msg->GetName().c_str());
            return false;
          }
          break;
        case LABEL_CT:
          msg->GetValue(strVal);
          lv_label_set_text(ctx->obj, strVal.c_str());
          break;
        case TEXTAREA_CT:
          msg->GetValue(strVal);
          // printf("setting %s to %s\n", msg->GetName().c_str(), strVal.c_str());
          lv_textarea_set_text(ctx->obj, strVal.c_str());
          break;
        case CALENDAR_CT:
          {
            msg->GetValue(strVal);
            lv_calendar_date_t date;
            cJSON *item = cJSON_Parse(strVal.c_str());

            date.day = cJSON_GetNumberValue(cJSON_GetObjectItem(item, "day"));
            date.month = cJSON_GetNumberValue(cJSON_GetObjectItem(item, "month")) + 1;
            date.year = cJSON_GetNumberValue(cJSON_GetObjectItem(item, "year"));

            lv_calendar_set_showed_date(ctx->obj, date.year, date.month);
            lv_calendar_set_today_date(ctx->obj, date.year, date.month, date.day);

            cJSON_Delete(item);
          }
          break;
        case CHECKBOX_CT:
          if (msg->GetValue(intVal))
          {
            if (intVal)
            {
              lv_obj_add_state(ctx->obj, LV_STATE_CHECKED);
            }
            else
            {
              lv_obj_clear_state(ctx->obj, LV_STATE_CHECKED);
            }
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume name (%s) value must be integer", msg->GetName().c_str());
            return false;
          }
          break;
        case COLORWHEEL_CT:
          if (msg->GetValue(intVal))
          {
            lv_color_t color;
            color.full = intVal;
            lv_colorwheel_set_rgb(ctx->obj, color);
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume name (%s) value must be (RGB encoded) integer", msg->GetName().c_str());
            return false;
          }
          break;
        case DROPDOWN_CT:
          msg->GetValue(strVal);
          lv_dropdown_set_text(ctx->obj, strVal.c_str());
          break;
        case ROLLER_CT:
          // TODO can't set selection from string
          // msg->GetValue(strVal);
          // lv_roller_set_selected(ctx->obj, strVal.c_str());
          // break;
        case IMGBUTTON_CT:
        case KEYBOARD_CT:
          ESP_LOGE(TAG.c_str(), "consume type (%d) NOT IMPLEMENTED", ctx->type);
          return false;
        case SLIDER_CT:
          if (msg->GetValue(intVal))
          {
            lv_slider_set_value(ctx->obj, intVal, LV_ANIM_OFF);
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume name (%s) value must be integer", msg->GetName().c_str());
            return false;
          }
          break;
        case SWITCH_CT:
          if (msg->GetValue(intVal))
          {
            if (intVal)
            {
              lv_obj_add_state(ctx->obj, LV_STATE_CHECKED);
            }
            else
            {
              lv_obj_clear_state(ctx->obj, LV_STATE_CHECKED);
            }
          }
          else
          {
            ESP_LOGE(TAG.c_str(), "consume name (%s) value must be integer", msg->GetName().c_str());
            return false;
          }
          break;
        default:
          ESP_LOGE(TAG.c_str(), "consume type (%d) NOT IMPLEMENTED", ctx->type);
          return false;
        }
      }
      else
      {
        ESP_LOGI(TAG.c_str(), "consume name (%s) NOT REGISTERED", msg->GetName().c_str());
        return false;
      }
      return true;
    }
  }

protected:
  static void produce_cb(lv_event_t *event)
  {
    ObjMsgDataRef data;
    LvglHost *host = (LvglHost *)event->user_data;
    control_reg_def_t *ctx = host->GetProducer(event->current_target);
    if (ctx)
    {
      if (lv_event_get_code(event) == ctx->eventCode)
      {
        switch (ctx->type)
        {
        case ARC_CT:
          data = ObjMsgDataInt::Create(
              host->origin_id, ctx->name, lv_arc_get_value(ctx->obj));
          host->Produce(data);
          break;
        case BUTTON_CT:
          data = ObjMsgDataInt::Create(host->origin_id, ctx->name,
                                       (lv_obj_get_state(ctx->obj) & LV_STATE_PRESSED) ? 1 : 0);
          host->Produce(data);
          break;
        case LABEL_CT:
          data = ObjMsgDataString::Create(
              host->origin_id, ctx->name, lv_label_get_text(ctx->obj));
          host->Produce(data);
          break;
        case TEXTAREA_CT:
          data = ObjMsgDataString::Create(
              host->origin_id, ctx->name, lv_textarea_get_text(ctx->obj));
          host->Produce(data);
          break;
        case CALENDAR_CT:
        {
          char buffer[60];
          lv_calendar_date_t date;
          lv_calendar_get_pressed_date(ctx->obj, &date);
          sprintf(buffer, "{ \"year:\" %d, \"month:\" %d, \"day:\" %d }",
                  date.year, date.month - 1, date.day);
          data = ObjMsgDataString::Create(
              host->origin_id, ctx->name, buffer, true);
          host->Produce(data);
          break;
        }
        case CHECKBOX_CT:
          data = ObjMsgDataInt::Create(host->origin_id, ctx->name,
                                       (lv_obj_get_state(ctx->obj) & LV_STATE_CHECKED) ? 1 : 0);
          host->Produce(data);
          break;
        case COLORWHEEL_CT:
        {
          lv_color_t color = lv_colorwheel_get_rgb(ctx->obj);
          data = ObjMsgDataInt::Create(
              host->origin_id, ctx->name, color.full);
          host->Produce(data);
          break;
        }
        case DROPDOWN_CT:
        {
          char buf[40];
          lv_dropdown_get_selected_str(ctx->obj, buf, sizeof(buf));
          data = ObjMsgDataString::Create(host->origin_id, ctx->name, buf);
          host->Produce(data);
          break;
        }
        case ROLLER_CT:
        {
          char buf[40];
          lv_roller_get_selected_str(ctx->obj, buf, sizeof(buf));
          data = ObjMsgDataString::Create(
              host->origin_id, ctx->name, buf);
          host->Produce(data);
          break;
        }
        case IMGBUTTON_CT:
        case KEYBOARD_CT:
          ESP_LOGE(host->TAG.c_str(), "produce type (%d) NOT IMPLEMENTED", ctx->type);
          break;
        case SLIDER_CT:
          data = ObjMsgDataInt::Create(
              host->origin_id, ctx->name, lv_slider_get_value(ctx->obj));
          host->Produce(data);
          break;
        case SWITCH_CT:
          data = ObjMsgDataInt::Create(
              host->origin_id, ctx->name, lv_obj_has_state(ctx->obj, LV_STATE_CHECKED) ? 1 : 0);
          host->Produce(data);
          break;
        default:
          ESP_LOGE(host->TAG.c_str(), "produce type (%d) NOT IMPLEMENTED", ctx->type);
          break;
        }
      }
      else
      {
        printf("Ignoring \"%s\" event %d\n", ctx->name, event->code);
      }
    }
    else
    {
      ESP_LOGE(host->TAG.c_str(), "produce event for %p NOT REGISTERED", event->target);
    }
  }

  control_reg_def_t *GetConsumer(string name)
  {
    unordered_map<string, control_reg_def_t>::iterator found = consume_map.find(name);
    if (found != consume_map.end())
    {
      return &found->second;
    }
    else
    {
      return NULL;
    }
  }
  lvglVirtualComsumer GetVirtualConsumer(string name)
  {
    unordered_map<string, lvglVirtualComsumer>::iterator found = virtual_consume_map.find(name);
    if (found != virtual_consume_map.end())
    {
      return found->second;
    }
    else
    {
      return NULL;
    }
  }

  control_reg_def_t *GetProducer(lv_obj_t *control)
  {
    unordered_map<lv_obj_t *, control_reg_def_t>::iterator found = produce_map.find(control);
    if (found != produce_map.end())
    {
      return &found->second;
    }
    else
    {
      return NULL;
    }
  }
  std::unordered_map<std::string, control_reg_def_t> consume_map;
  std::unordered_map<std::string, lvglVirtualComsumer> virtual_consume_map;
  std::unordered_map<lv_obj_t *, control_reg_def_t> produce_map;
};
