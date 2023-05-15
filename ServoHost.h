/** @file
 *
 * The ServoHost class provides for operation of
 * servo motors
 *
 */
#pragma once
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

#include "ObjMsg.h"

#define SERVO_MIN_PW_US 500               // Minimum pulse width in microsecond
#define SERVO_MAX_PW_US 2500              // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE -90              // Minimum angle
#define SERVO_MAX_DEGREE 90               // Maximum angle

#define SERVO_PULSE_GPIO 0                // GPIO connects to the PWM signal line
#define SERVO_TIME_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIME_PERIOD 20000           // 20000 ticks, 20ms


typedef ObjMsgDataInt ObjMsgServoData;

class ServoHost : public ObjMsgHost
{
public:
  ServoHost(ObjMsgTransport &transport, uint16_t origin)
      : ObjMsgHost(transport, "SERVO", origin)
  {
    timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIME_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = SERVO_TIME_PERIOD,
        .flags {
          .update_period_on_empty = 0,
          .update_period_on_sync = 0,
        }
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group as the timer
        .flags { 
          .update_gen_action_on_tez = 0,
          .update_gen_action_on_tep = 0,
          .update_gen_action_on_sync = 0,
          .update_dead_time_on_tez = 0,
          .update_dead_time_on_tep = 0,
          .update_dead_time_on_sync = 0,
         }
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG.c_str(), "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));
  }

  static inline uint32_t AngleToPulsewidth(int angle)
  {
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PW_US 
      - SERVO_MIN_PW_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE)
      + SERVO_MIN_PW_US;
  }

  int Add(const char *name, gpio_num_t pin)
  {
    ObjMsgData::RegisterClass(origin_id, name, ObjMsgServoData::Create);

    ESP_LOGI(TAG.c_str(), "Create comparator and generator from the operator");
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags{
            .update_cmp_on_tez = true,
            .update_cmp_on_tep = false,
            .update_cmp_on_sync = false,
        }};
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = pin,
        .flags {
          .invert_pwm = 0,
          .io_loop_back= 0,
        }
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, 
      AngleToPulsewidth(0)));
    ESP_LOGI(TAG.c_str(), "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_timer_event(generator,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
      MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
      MCPWM_GEN_TIMER_EVENT_ACTION_END()));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_actions_on_compare_event(generator,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, 
        MCPWM_GEN_ACTION_LOW),
      MCPWM_GEN_COMPARE_EVENT_ACTION_END()));
    // Create a servo object and map it by name
    servos[name] = Servo(name, pin, comparator);

    return 0;
  }

  bool Start()
  {
    ESP_LOGI(TAG.c_str(), "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    return true;
  }

  bool Consume(ObjMsgData *data)
  {
    ObjMsgServoData *point = static_cast<ObjMsgServoData *>(data);
    if (point)
    {
      Servo *servo = GetServo(point->GetName());
      if (servo) {
        int angle;
        point->GetRawValue(angle);
        angle = std::min(std::max(angle, servo->minAngle), servo->maxAngle);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(servo->comparator, 
          AngleToPulsewidth(angle)));
        return true;
      }
    }
    return false;
  }

protected:
  class Servo
  {
  public:
    const char *name; /**< Servo name */
    gpio_num_t pin;   /**< GPIO Pin connected to servo */
    mcpwm_cmpr_handle_t comparator;
    Servo(){} // default constructor required for map
    Servo(const char *name, gpio_num_t pin, mcpwm_cmpr_handle_t comparator,
      int16_t maxAngle = 90, int16_t minAngle = -90)
        : name(name), pin(pin), comparator(comparator),
          maxAngle(maxAngle), minAngle(minAngle) {}
    int maxAngle;     /**< Maximum angle in degrees */
    int minAngle;     /**< Minimum angle in degrees */
  };

  Servo *GetServo(string name) {
      unordered_map<string, Servo>::iterator found = servos.find(name);
      if (found != servos.end()) {
          return &found->second;
      }
      else {
          return NULL;
      }
  }

  unordered_map<string, Servo> servos;
  mcpwm_oper_handle_t oper = NULL;
  mcpwm_timer_handle_t timer = NULL;
};
