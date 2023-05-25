#include "ObjMsg.h"

#include "GpioHost.h"
#include "JoystickHost.h"
#include "ServoHost.h"
#include "Esp32IoAdapt.h"

#include "WebsocketHost.h"

#include <unordered_map>

#define TAG "APP"

// Pan-tilt Slider and joystick pin/channel assignments
#define PT_SLIDER IA_GVI1_CHAN

#define PT_JOY_PINS IA_GVIIS1_PINS
#define PT_JOY_CHANS IA_GVIIS1_CHANS

// Zoom Slider and joystick pin/channel assignments
#define ZOOM_SLIDER IA_GVI2_CHAN

#define ZOOM_JOY_PINS IA_GVSSS1_PINS
#define ZOOM_JOY_CHANS IA_GVSSS1_CHANS

// Servo pin assignments
#define ZOOM_SERVO_X_PIN IA_GVS1_PIN

// LED Output pins
#define LED_OUT_PINS IA_GVSS1_PINS

// Sample intervals
#define SAMPLE_INTERVAL_MS 100

// Component names
#define PT_JOY_NAME "pantilt"
#define PT_SLIDER_NAME "pantilt_slider"
#define ZOOM_JOY_NAME "zoom"
#define ZOOM_SLIDER_NAME "zoom_slider"
#define ZOOM_SERVO_X_NAME "zoom_servo_x"

// Define LED for WebsocketHost
#define WEBSOCK_LED LED_OUT_PINS[0] // GPIO_NUM_NC to not use

// Origin IDs
enum Origins
{
  ORIGIN_CONTROLLER,
  ORIGIN_JOYSTICK,
  ORIGIN_SERVO,
  ORIGIN_ADC,
  ORIGIN_WEBSOCKET,
  ORIGIN_LVGL,
  ORIGIN_GPIO,
};

const char *origins[] = {
  "ORIGIN_CONTROLLER",
  "ORIGIN_JOYSTICK",
  "ORIGIN_SERVO",
  "ORIGIN_ADC",
  "ORIGIN_WEBSOCKET",
  "ORIGIN_LVGL",
  "ORIGIN_GPIO",
};

// 
TaskHandle_t MessageTaskHandle;

// Transport
ObjMsgTransport transport(MSG_QUEUE_MAX_DEPTH);

// Component hosts
AdcHost adc(transport, ORIGIN_ADC, SAMPLE_INTERVAL_MS);
GpioHost gpio(transport, ORIGIN_GPIO);

JoystickHost joysticks(adc, gpio, transport, ORIGIN_JOYSTICK, SAMPLE_INTERVAL_MS);
ServoHost servos(transport, ORIGIN_SERVO);
WebsocketHost ws(transport, ORIGIN_WEBSOCKET, WEBSOCK_LED, false);

// Implementation
static void MessageTask(void *pvParameters)
{
  ObjMsgDataRef dataRef;
  TickType_t wait = portMAX_DELAY;

  for (;;)
  {
    if (transport.Receive(dataRef, wait))
    {
      ObjMsgData *data = dataRef.get();
      ObjMsgJoystickData *jsd = static_cast<ObjMsgJoystickData *>(data);
      if (jsd && jsd->GetName().compare(ZOOM_JOY_NAME) == 0)
      {
        joystick_sample_t sample;
        jsd->GetRawValue(sample);
        ObjMsgServoData servo(jsd->GetOrigin(), ZOOM_SERVO_X_NAME, sample.x);
        servos.Consume(&servo);
        ObjMsgDataInt x(jsd->GetOrigin(), "zoom_x", sample.x > 0);
        gpio.Consume(&x);
        ObjMsgDataInt y(jsd->GetOrigin(), "zoom_y", sample.y > 0);
        gpio.Consume(&y);

      }
      string str;
      data->Serialize(str);
      ESP_LOGI(TAG, "(%s) JSON: %s", origins[data->GetOrigin()], str.c_str());
    }
  }
}

void MessagingInit()
{
  // Configure and start joysticks
  joysticks.Add(PT_JOY_NAME, CHANGE_EVENT,
                PT_JOY_CHANS[1], PT_JOY_CHANS[0], PT_JOY_PINS[2]);
  joysticks.Add(ZOOM_JOY_NAME, CHANGE_EVENT,
                ZOOM_JOY_CHANS[1], ZOOM_JOY_CHANS[0], ZOOM_JOY_PINS[2]);
  joysticks.Start();

  // Configure and start servos
  servos.Add(ZOOM_SERVO_X_NAME, ZOOM_SERVO_X_PIN);
  servos.Start();

  // Configure and start ADC
  adc.Add(ZOOM_SLIDER_NAME, CHANGE_EVENT, ZOOM_SLIDER,
          ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, 0, 100);
  adc.Add(PT_SLIDER_NAME, CHANGE_EVENT, PT_SLIDER,
          ADC_ATTEN_DB_11, ADC_BITWIDTH_12, 4096, 0, 100);
  adc.Start();

  // Configure and start GPIO
  gpio.Add("zoom_x", LED_OUT_PINS[0], POLLING, DEFAULT_GF);
  gpio.Add("zoom_y", LED_OUT_PINS[1], POLLING, DEFAULT_GF);
  gpio.Start();

  xTaskCreate(MessageTask, "MessageTask",
              CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE + 1024, NULL,
              tskIDLE_PRIORITY, &MessageTaskHandle);

  // Configure and start wifi / http / websocket
  extern void HttpPaths(WebsocketHost & ws);
  HttpPaths(ws);
  // WARNING - (for now) do this last. It will not return until WiFi starts
  ws.Start();
  // Have transport forward messages to ws
  transport.AddForward(&ws);
}

extern "C" void app_main(void)
{
  MessagingInit();
}