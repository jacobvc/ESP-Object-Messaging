#include "ObsWsClientHost.h"

class AvDeviceWsClientHost : public ObsWsClientHost
{
public:
  class AvdClientInterface : public WsClientInterface 
  {
public:
    AvdClientInterface(string name, string uri, AvDeviceWsClientHost* host)
      : WsClientInterface(name, uri, host) {}

    int CameraDisconnect(const char* device) {
      return CameraRequest("Disconnect", device);
    }
    int CameraConnect(const char* device) {
      return CameraRequest("Connect", device);
    }
    esp_err_t GetAvDevices() {
      return Request("GetAvDevices");
    }
    /*
           "MovePtz",
                 cameraname,
                 direction ("up", "down", "left", "right", "stop")
                 amount 0 to 100
     */
    int MovePtz(const char* device, const char* dir, int amount) {
      return CameraDirectionAmountRequest("MovePtz", device, dir, amount);
    }
    /*         "Zoom",
                   cameraname,
                   direction ("in", "out", "stop")
                   amount 0 to 100
     */
    int Zoom(const char* device, const char* dir, const int amount) {
      return CameraDirectionAmountRequest("Zoom", device, dir, amount);
    }

    /*       "Preset", cameraname, preset
    */
    int Preset(const char* device, const char* preset) {
      cJSON* reqData = cJSON_CreateObject();
      cJSON_AddStringToObject(reqData, "cameraname", device);
      cJSON_AddStringToObject(reqData, "preset", preset);
      return Request("Preset", reqData);
    }
    /*
          "Mute", mixername channelname value (bool)
     */
    int Mute(const char* device, const char* chan, const char* value) {
      return MixerChannelValueRequest("Zoom", device, chan, value);
    }
    /*
          "VolumeSetting" mixername channelname value (0 to 100)
     */
    int VolumeSetting(const char* device, const char* chan, const char* value) {
      return MixerChannelValueRequest("VolumeSetting", device, chan, value);
    }
  protected:
    int MixerChannelValueRequest(const char* request, const char* device, const char* chan, const char* value) {
      cJSON* reqData = cJSON_CreateObject();
      cJSON_AddStringToObject(reqData, "mixername", device);
      cJSON_AddStringToObject(reqData, "channelname", chan);
      cJSON_AddStringToObject(reqData, "value", value);
      return Request(request, reqData);
    }
    int CameraDirectionAmountRequest(const char* request, const char* device, const char* dir, int amount) {
      cJSON* reqData = cJSON_CreateObject();
      cJSON_AddStringToObject(reqData, "cameraname", device);
      cJSON_AddStringToObject(reqData, "direction", dir);
      cJSON_AddNumberToObject(reqData, "amount", amount);
      return Request(request, reqData);
    }
    int CameraRequest(const char* request, const char* device) {
      cJSON* reqData = cJSON_CreateObject();
      cJSON_AddStringToObject(reqData, "cameraname", device);
      return Request(request, reqData);
    }
  };

  AvDeviceWsClientHost(ObjMsgTransport* transport, uint16_t origin)
    : ObsWsClientHost(transport, origin) {}

  AvdClientInterface *Add(string name, const char* url)
  {
    interfaces[name] = new AvdClientInterface(name, url, this);

    ObjMsgData::RegisterClass(origin_id, name, ObjMsgDataJson::Create);

    return (AvdClientInterface *)interfaces[name];
  }

};