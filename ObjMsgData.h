/** @file
 *
 * Declaration of ObjMsgData and derived scalar classes
 *
 */
#pragma once

#define isnan(n) ((n != n) ? 1 : 0)

using namespace std;

#define CORE_TAG "ObjMsg"

/***
 *       ___  _     _ __  __         ___       _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|
 *               |__/          |___/
 */

/**
 */
class ObjMsgData; // Forward reference
typedef shared_ptr<ObjMsgData> ObjMsgDataRef;

/** ObjMsgData virtual base class
 *
 * All data is exchanged using objects of the ObjMsgData base class
 *
 * Identifies origin, endpoint name, and contains
 * binary data with JSON serialization and deserialization.
 *
 * Abstract base class for templatized ObjMsgDataT. Intended to always be
 * instantiated using a ObjMsgDataRef (a std::shared_ptr)
 */
class ObjMsgData
{
  protected:
  /** ID of message originator */
  uint16_t origin;
  /** Endpoint name */
  string name;

public:
  /** MANDATORY vitrual destructor */
  virtual ~ObjMsgData() {}

  uint16_t GetOrigin() { return origin; }
  string &GetName() { return name; }
  /** Populate value from the contents of 'json'
   */
  virtual bool deserializeValue(cJSON *json) = 0;

  /** serialize this object into a JSON string */
  virtual int serializeJson(string &json) = 0;

  /** Create a string that represents this object */
  virtual void tostring(string &str) = 0;

  /** Create a string that conveys the value of this object
   *
   * This will contain, for example, a number for a scalar numeric
   * value, or a JSON object for a complex value.
   */
  virtual void serializeValue(string &str) = 0;
};

/***
 *       ___  _     _ __  __         ___       _       _____
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ |_   _|
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` || |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_||_|
 *               |__/          |___/
 */

/** Templatized ObjMsgData class
 */
template <class T>
class ObjMsgDataT : public ObjMsgData
{
protected:
  /** The (template specific) binary value */
  T value;
  /** Constructor */
  ObjMsgDataT(uint16_t origin, char const *name)
  {
    this->origin = origin;
    this->name = name;
  }

public:
  void GetValue(T &out) { out = value; }

  // Default implementation (unquoted for numbers and objects)
  int serializeJson(string &json)
  {
    string val;
    serializeValue(val);
    json = "{\"name\":\"" + name + "\", \"value\":" + val + "}";

    return 0;
  }

  void tostring(string &str)
  {
    string json;
    serializeJson(json);
    str = json;
  }

  /** MANDATORY vitrual destructor */
  ~ObjMsgDataT() {}
};

/***
 *       ___  _     _ __  __         ___       _        _   _ _     _   ___
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _| | | (_)_ _| |_( _ )
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` | |_| | | ' \  _/ _ \
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|\___/|_|_||_\__\___/
 *               |__/          |___/
 */

/** ObjMsgData with 8 bit unsigned integer scalar value */
class ObjMsgDataUInt8 : public ObjMsgDataT<uint8_t>
{
protected:
public:
  // TODO make constructors protected for all of these derived types.
  // Skip for now. It is a bit messy and not part of the primary task
  ObjMsgDataUInt8(uint16_t origin, char const *name, uint8_t value)
      : ObjMsgDataT<uint8_t>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgDataUInt8(%u, %s, %u) constructed", origin, name, value);
  }
  ~ObjMsgDataUInt8()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgDataUInt8 destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name, uint8_t value)
  {
    return std::make_shared<ObjMsgDataUInt8>(origin, name, value);
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgDataUInt8>(origin, name, 0);
  }

  // Common implementation for integer values
  bool deserializeValue(cJSON *json)
  {
    cJSON *valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj)
    {
      value = valueObj->valueint;
      return true;
    }
    return false;
  }

  void serializeValue(string &str)
  {
    char buffer[32];
    sprintf(buffer, "%u", value);
    str = buffer;
  }
};

/***
 *       ___  _     _ __  __         ___       _        ___     _   
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _|_ _|_ _| |_
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` || || ' \  _|
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|___|_||_\__|
 *               |__/          |___/
 */

/** ObjMsgData with integer scalar value */
class ObjMsgDataInt : public ObjMsgDataT<int>
{
protected:
public:
  ObjMsgDataInt(uint16_t origin, char const *name, int value)
      : ObjMsgDataT<int>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgDataInt(%u, %s, %d) constructed", origin, name, value);
  }

  ~ObjMsgDataInt()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgDataInt destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name, int value)
  {
    return std::make_shared<ObjMsgDataInt>(origin, name, value);
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgDataInt>(origin, name, 0);
  }

  // Common implementation for integer values
  bool deserializeValue(cJSON *json)
  {
    cJSON *valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj)
    {
      value = valueObj->valueint;
      return true;
    }
    return false;
  }

  void serializeValue(string &str)
  {
    char buffer[32];
    sprintf(buffer, "%ld", value);
    str = buffer;
  }
};

/***
 *       ___  _     _ __  __         ___       _        ___ _           _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _| __| |___  __ _| |_
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` | _|| / _ \/ _` |  _|
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|_| |_\___/\__,_|\__|
 *               |__/          |___/
 */

/** ObjMsgData with floating point scalar value */
class ObjMsgDataFloat : public ObjMsgDataT<double>
{
public:
  ObjMsgDataFloat(uint16_t origin, char const *name, double value)
      : ObjMsgDataT<double>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgDataFloat(%u, %s, %f) constructed", origin, name, value);
  }

  ~ObjMsgDataFloat()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgDataFloat destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgDataFloat>(origin, name, 0);
  }
  static ObjMsgDataRef create(uint16_t origin, char const *name, double value)
  {
    return std::make_shared<ObjMsgDataFloat>(origin, name, value);
  }

  bool deserializeValue(cJSON *json)
  {
    cJSON *valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj)
    {
      double tmp = valueObj->valuedouble;
      if (!isnan(tmp))
      {
        value = tmp;
        return true;
      }
    }
    return false;
  }

  void serializeValue(string &str)
  {
    char buffer[32];
    sprintf(buffer, "%f", value);
    str = buffer;
  }
};

/***
 *       ___  _     _ __  __         ___       _        ___ _       _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _/ __| |_ _ _(_)_ _  __ _
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` \__ \  _| '_| | ' \/ _` |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|___/\__|_| |_|_||_\__, |
 *               |__/          |___/                                      |___/
 */

/** ObjMsgData with std::string object value */
class ObjMsgDataString : public ObjMsgDataT<string>
{
protected:
public:
  ObjMsgDataString(uint16_t origin, char const *name, const char *value)
      : ObjMsgDataT<string>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgDataString(%u, %s, %u) constructed", origin, name, value);
  }
  ~ObjMsgDataString()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgDataString destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name, const char *value)
  {
    return std::make_shared<ObjMsgDataString>(origin, name, value);
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name)
  {
    return std::make_shared<ObjMsgDataString>(origin, name, "");
  }

  bool deserializeValue(cJSON *json)
  {
    cJSON *valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj)
    {
      value = valueObj->valuestring;
      return true;
    }
    return false;
  }

  // Quoted value for strings
  int serializeJson(string &json)
  {
    string val;
    serializeValue(val);
    json = "{\"name\":\"" + name + "\", \"value\":\"" + val + "\"}";

    return 0;
  }

  void serializeValue(string &str)
  {
    str = value;
  }
};
