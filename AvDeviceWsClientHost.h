#include "ObsWsClientHost.h"

/// @brief An ObjWsClientHost, hosting a websocket client interface for
/// an AvDeviceControl object 
///
/// Supports Obs Websocket protocol over a websocket connection.
///
/// Implements commands as function calls and 
/// Delivers received content using ObjMsgDataJson.
class AvDeviceWsClientHost : public ObsWsClientHost
{
public:
  /// @brief Interface for connection instance
  class AvdClientInterface : public WsClientInterface 
  {
public:
    // Constructor
    AvdClientInterface(string name, string uri, AvDeviceWsClientHost* host)
      : WsClientInterface(name, uri, host) {}

    /// @brief  Request for AvDeviceControl to Disconnect specified device
    /// @param device - Device name to disconnect
    /// @return ESP_OK or error value
    int CameraDisconnect(const char* device) {
      return CameraRequest("Disconnect", device);
    }
    /// @brief  Request for AvDeviceControl to Connect specified device
    /// @param device - Device name to connect
    /// @return ESP_OK or error value
    int CameraConnect(const char* device) {
      return CameraRequest("Connect", device);
    }

    /// @brief Request AvDeviceControl device configuration
    /// @return ESP_OK or error value
    esp_err_t GetAvDevices() {
      return Request("GetAvDevices");
    }

    /// @brief Move the PTZ device (one of pan or tilt)
    /// @param device - Device name to move
    /// @param dir "up", "down", "left", "right", "stop"
    /// @param amount - move speed, percent, 0 to 100
    /// @return ESP_OK or error value
    int MovePtz(const char* device, const char* dir, int amount) {
      return CameraDirectionAmountRequest("MovePtz", device, dir, amount);
    }

    /// @brief - Change device zoom
    /// @param device - Device to zoom
    /// @param dir - Direction: "in", "out", "stop"
    /// @param amount - zoom speed, percent, 0 to 100
    /// @return ESP_OK or error value
    int Zoom(const char* device, const char* dir, const int amount) {
      return CameraDirectionAmountRequest("Zoom", device, dir, amount);
    }

    /// @brief Select a device preset
    /// @param device - Name of device
    /// @param preset - Name of preset
    /// @return ESP_OK or error value
    int Preset(const char* device, const char* preset) {
      cJSON* reqData = cJSON_CreateObject();
      cJSON_AddStringToObject(reqData, "cameraname", device);
      cJSON_AddStringToObject(reqData, "preset", preset);
      return Request("Preset", reqData);
    }

    /// @brief Mute 'chan' of 'device', on or off
    /// @param device - Name of device
    /// @param chan - Device channel
    /// @param value - On / Off
    /// @return ESP_OK or error value
    int Mute(const char* device, const char* chan, const char* value) {
      return MixerChannelValueRequest("Zoom", device, chan, value);
    }

    /// @brief Change volume setting of 'chan on 'device' to 'value'
    /// @param device - Name of device
    /// @param chan - Device channel
    /// @param value - New value
    /// @return ESP_OK or error value
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

  /// @brief  Constructor
  /// @param transport 
  /// @param origin 
  AvDeviceWsClientHost(ObjMsgTransport* transport, uint16_t origin)
    : ObsWsClientHost(transport, origin) {}

  /// @brief Add connection to 'uri; named 'name'
  /// @param name - Name of data object
  /// @param uri - Websocket connection endpoint
  /// @return - pointer too new AvdClientInterface
  AvdClientInterface *Add(string name, const char* uri)
  {
    interfaces[name] = new AvdClientInterface(name, uri, this);

    ObjMsgData::RegisterClass(origin_id, name, ObjMsgDataJson::Create);

    return (AvdClientInterface *)interfaces[name];
  }

};