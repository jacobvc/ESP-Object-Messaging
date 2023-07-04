#include "ObjMsg.h"
#include "GpioHost.h"
#include <unordered_map>

#define TAG "APP"

// Define LED for WebsocketHost
#define LED_BUILTIN GPIO_NUM_2
#define GPIO_BOOT GPIO_NUM_0

// Origin IDs 
enum Origins
{
  ORIGIN_CONTROLLER,
  ORIGIN_GPIO,
};

const char *origins[] = {
  "ORIGIN_CONTROLLER",
  "ORIGIN_GPIO",
};

// 
TaskHandle_t MessageTaskHandle;

// Transport
ObjMsgTransport transport(MSG_QUEUE_MAX_DEPTH);

// ObjMsgHosts
GpioHost gpio(&transport, ORIGIN_GPIO);

//
// Message Task
//
static void MessageTask(void *pvParameters)
{
  ObjMsgDataRef dataRef;
  TickType_t wait = portMAX_DELAY;

  for (;;)
  {
    if (transport.Receive(dataRef, wait))
    {
      ObjMsgData *btnData = dataRef.get();
      if (btnData->GetName().compare("bootBtn") == 0)
      {
        // Send 'bootBtn' value to 'builtinLed'
        int value;
        btnData->GetValue(value);
        ObjMsgDataInt led(btnData->GetOrigin(), "builtinled", value);
        gpio.Consume(&led);
      }
      // Show all of the messages
      string str;
      btnData->Serialize(str);
      ESP_LOGI(TAG, "(%s) JSON: %s", origins[btnData->GetOrigin()], str.c_str());
    }
  }
}

//
// Entry point
//
extern "C" void app_main(void)
{
  // Configure and start GPIOs
  gpio.Add("builtinled", LED_BUILTIN, POLLING, DEFAULT_GF);
  gpio.Add("bootBtn", GPIO_BOOT, CHANGE_EVENT, 
    NEG_EVENT_GF | POS_EVENT_GF | IS_INPUT_GF | INVERTED_GF);
  gpio.Start();

  xTaskCreate(MessageTask, "MessageTask",
    CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE + 1024, NULL,
    tskIDLE_PRIORITY, &MessageTaskHandle);
}
