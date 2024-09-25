#pragma once

#include <stdint.h>
#include "libvisca.h"
#include "Joystick3AxisHost.h"

/*  pan_speed should be in the range 01 - 18.
    tilt_speed should be in the range 01 - 14
    pan_position should be in the range -880 - 880 (0xFC90 - 0x370)
    tilt_position should be in range -300 - 300 (0xFED4 - 0x12C)
*/
#define PAN_SPEED_MAX 18
#define TILT_SPEED_MAX 14
#define PAN_POS_MAX 880
#define TILT_POS_MAX 300
#define ZOOM_MIN_SPEED 2
#define ZOOM_MAX_SPEED 7

#define SIGN(x) ((x < 0) ? -1 : 1)
//#define ZOOM_SPEED(x) ((ZOOM_MAX_SPEED - ZOOM_MIN_SPEED + 1) * adc1_get_raw(ZOOM_SLIDER) / SLIDER_MAX * (x - 1) / 9 + ZOOM_MIN_SPEED)
#define ZOOM_SPEED(x) ((ZOOM_MAX_SPEED - ZOOM_MIN_SPEED) * (x - 1) / 99 + ZOOM_MIN_SPEED)

/// @brief  An ObjMsgHost object, hosting VISCA interfaces
class ViscaHost : public ObjMsgHost
{
public:
  class ViscaInterface {
  public:
    string name;
    ViscaInterface(string name, uart_port_t device, int rxpin, int txpin, bool autoConnect)
      : name(name) {
      if (VISCA_configure_serial(&intf, device, rxpin, txpin) != VISCA_SUCCESS) {
      }
      intf.autoConnect = autoConnect;
      intf.broadcast = 0;
      camera.address = 1;

    }
    ViscaInterface(string name, const char* ip, int port, bool autoConnect)
      : name(name) {
      if (VISCA_configure_tcp(&intf, ip, port) != VISCA_SUCCESS) {
      }
      intf.autoConnect = autoConnect;
      intf.broadcast = 0;
      camera.address = 1;
    }
    VISCAInterface_t intf;
    VISCACamera_t camera;

    bool Open()
    {
      int camera_num;

      if (VISCA_open_interface(&intf) != VISCA_SUCCESS) {
        ESP_LOGE("VISCA", "Unable to open interface.");
        return false;
      }
      else {
        // Initialize VISCA interface
        VISCA_set_address(&intf, &camera_num);
        VISCA_clear(&intf, &camera);

        print_camera_info(this);
        ESP_LOGI("VISCA", "Open success.\n");
        return true;
      }
    }

    bool IsConnected() {
      return intf.connected;
    }
  };

  QueueHandle_t visca_queue;
  ViscaInterface* selectedInterface;
  unordered_map<string, ViscaInterface*> interfaces;

  /// Constructor, specifying transport object and origin 
  /// @param transport: Transport object
  /// @param origin: Origin ID for this host
  ViscaHost(ObjMsgTransport* transport, uint16_t origin)
    : ObjMsgHost(transport, "VISCA", origin)
  {
    visca_queue = xQueueCreate(10, sizeof(Joystick3AxisSample_t));
    selectedInterface = NULL;

    xTaskCreate(Task, "visca_task",
      CONFIG_ESP_MINIMAL_SHARED_STACK_SIZE + 2048, this, 10, NULL);
  }

  ViscaInterface* Add(string name, const char* ip, uint16_t port, bool autoConnect = true)
  {
    interfaces[name] = new ViscaInterface(name, ip, port, autoConnect);
    // Select the last interface as active
    selectedInterface = interfaces[name];

    ObjMsgData::RegisterClass(origin_id, name, Joystick3AxisData::Create);

    return interfaces[name];
  }
  ViscaInterface* Add(string name, uart_port_t device, int rxpin, int txpin, bool autoConnect = true)
  {
    interfaces[name] = new ViscaInterface(name, device, rxpin, txpin, autoConnect);
    // Select the last interface as active
    selectedInterface = interfaces[name];

    ObjMsgData::RegisterClass(origin_id, name, Joystick3AxisData::Create);

    return interfaces[name];
  }

  bool Start()
  {
    bool result = true;

    return result;
  }

  /** Process a message intended for the VISCA camera control
 */
  bool Consume(ObjMsgData* data)
  {
    Joystick3AxisSample_t jd;

    // CHECK TO SEE IF IT IS OBJECT vs string
    // Is there a safer way to get the object??
    // joystick data is the object when id is ORIGIN_JOYSTICK_VIEW
    // AND encoding is JOYSTICK_DATA_STRUCT_ENC
    Joystick3AxisData* sample = static_cast<Joystick3AxisData*>(data);
    if (sample != NULL) {
      sample->GetRawValue(jd);
      xQueueSend(visca_queue, &jd, 0);
      return true;
    }
    else {
      return false;
    }
  }

  bool Select(string device) {
    unordered_map<string, ViscaInterface*>::iterator found = interfaces.find(device);
    if (found != interfaces.end()) {
      selectedInterface = found->second;
      return true;
    }
    else {
      return false;
    }
  }

  void Acton(ViscaInterface* vf, Joystick3AxisSample_t& js) {
    if (abs(js.x) < 5 && abs(js.y) < 5) {
      visca_pt_stop(vf);
    }
    else {
      visca_pt_move(vf, js.x, js.y);
    }
    // printf("zoom(%d:%d)\n", js.y, ZOOM_SPEED(js.y));
    if (abs(js.z) < 5) {
      VISCA_set_zoom_stop(&vf->intf, &vf->camera);
    }
    else if (js.z > 0) {
      //printf("zoomTele(%d=>%d)\n", js.z, ZOOM_SPEED(js.z));
      VISCA_set_zoom_tele_speed(&vf->intf, &vf->camera, ZOOM_SPEED(js.z));
    }
    else if (js.z < 0) {
      //printf("zoomWide(%d=>%d)\n", js.z, ZOOM_SPEED(-js.z));
      VISCA_set_zoom_wide_speed(&vf->intf, &vf->camera, ZOOM_SPEED(-js.z));
    }
  }
  /** Send serial VISCA messages to camera based on joystick samples
   * received in visca queue, considering measurement state of other
   * measurements.
   *
   * IT IS ASSUMED that a VISCA message will be sent for every reception
   * serial flow control should be asserted before putting a message
   * in the visca_queue
   */
  static void Task(void* arg)
  {
    ViscaHost* ep = (ViscaHost*)arg;
    for (;;) {
      Joystick3AxisSample_t js;

      do {
        xQueueReceive(ep->visca_queue, &js, portMAX_DELAY);
      } while (uxQueueMessagesWaiting(ep->visca_queue) > 0);
      //printf("(* %d %d %d)", js.x, js.y, js.z);

      if (ep->selectedInterface) {
        ep->Acton(ep->selectedInterface, js);
      }
      else {
        unordered_map<string, ViscaInterface*>::iterator it;
        for (it = ep->interfaces.begin(); it != ep->interfaces.end(); it++) {
          ViscaInterface* vf = it->second;
          ep->Acton(vf, js);
        }
      }
    }
  }
private:
  static void print_camera_info(ViscaInterface* vf)
  {
    int err = VISCA_get_camera_info(&vf->intf, &vf->camera);
    if (err == VISCA_SUCCESS) { // else Not supported by camera
      //printf("VISCA_get_camera_info=>err=%d, type=%d\n", err, vf->intf.type);
      ESP_LOGI("VISCA", "Some camera info:\n------------------\nvendor: 0x%04lx\n model: 0x%04lx\n ROM version: 0x%04lx\n socket number: 0x%02lx\n",
        vf->camera.vendor, vf->camera.model, vf->camera.rom_version, vf->camera.socket_num);
    }
  }

  void visca_pt_stop(ViscaInterface* vf)
  {
    VISCA_set_pantilt(&vf->intf, &vf->camera, 0, 0);
  }

  void visca_pt_move(ViscaInterface* vf, int x, int y)
  {
    //printf("move(%d=>%d, %d=>%d)\n", 
    //  x, x * PAN_SPEED_MAX / 100, 
    //  y, y * TILT_SPEED_MAX / 100);
    VISCA_set_pantilt(&vf->intf, &vf->camera,
      x * PAN_SPEED_MAX / 100 /* * adc1_get_raw(P_T_SLIDER) / (10 * SLIDER_MAX) */,
      y * TILT_SPEED_MAX / 100 /* * adc1_get_raw(P_T_SLIDER) / (SLIDER_MAX * 10) */);
  }

};


