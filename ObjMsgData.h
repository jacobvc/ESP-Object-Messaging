/** @file
 *
 * Declaration of ObjMsgData and derived scalar classes
 *
 */
#pragma once

#define isnan(n) ((n != n) ? 1 : 0)

using namespace std;

#define CORE_TAG "ObjMsg"

/*
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
  bool IsFrom(int16_t origin) { return this->origin == origin; }
  string &GetName() { return name; }
  /** Populate value from the contents of 'json'
   */
  virtual bool deserializeValue(cJSON *json) = 0;

  /** serialize this object into a JSON string */
  virtual int Serialize(string &json) = 0;

  /** Create a string that represents this object's value */
  virtual bool GetValue(string &str) = 0;
  virtual bool GetValue(int &val) = 0;
  virtual bool GetValue(double &val) = 0;

  /** Create a string that conveys the value of this object
   *
   * This will contain, for example, a number for a scalar numeric
   * value, or a JSON object for a complex value.
   */
};

/*
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
  bool GetRawValue(T &out) { out = value; return true; }

  // Default implementation (unquoted for numbers and objects)
  int Serialize(string &json)
  {
    string val;
    GetValue(val);
    json = "{\"name\":\"" + name + "\", \"value\":" + val + "}";

    return 0;
  }

  /** MANDATORY vitrual destructor */
  ~ObjMsgDataT() {}
};


/*
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

  bool GetValue(int &val)
  {
    val = value;
    return true;
  }
  bool GetValue(double &val)
  {
    val = value;
    return true;
  }
  bool GetValue(string &str)
  {
    char buffer[32];
    sprintf(buffer, "%ld", value);
    str = buffer;

    return true;
  }
};

/*
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

  bool GetValue(int &val)
  {
    val = value;
    return true;
  }
  bool GetValue(double &val)
  {
    val = value;
    return true;
  }
  bool GetValue(string &str)
  {
    char buffer[32];
    sprintf(buffer, "%f", value);
    str = buffer;

    return true;
  }
};

/*
 *       ___  _     _ __  __         ___       _        ___ _       _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _/ __| |_ _ _(_)_ _  __ _
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` \__ \  _| '_| | ' \/ _` |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|___/\__|_| |_|_||_\__, |
 *               |__/          |___/                                      |___/
 */

/** ObjMsgData with std::string object value 
 * 
 * Note that if this is constructed with asJson = True, the
 * (presumably json) string value is serialized without quotes.
 */
class ObjMsgDataString : public ObjMsgDataT<string>
{
protected:
  bool asJson;
public:
  ObjMsgDataString(uint16_t origin, char const *name, const char *value, bool asJson = false)
      : ObjMsgDataT<string>(origin, name), asJson(asJson)
  {
    this->value = value;
    // ESP_LOGI(CORE_TAG, "ObjMsgDataString(%u, %s, %u) constructed", origin, name, value);
  }
  ~ObjMsgDataString()
  {
    // ESP_LOGI(CORE_TAG, "ObjMsgDataString destructed");
  }

  static ObjMsgDataRef create(uint16_t origin, char const *name, const char *value, bool asJson = false)
  {
    return std::make_shared<ObjMsgDataString>(origin, name, value, asJson);
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
  int Serialize(string &json)
  {
    string val;
    GetValue(val);
    json = "{\"name\":\"" + name + "\", \"value\":";
    if (asJson) {
      json += val;
    }
    else {
      json += "\"" + val + "\"";
    }
    json += " }";

    return 0;
  }

  // TODO try parsing and return true if value is a number
  bool GetValue(int &val)
  {
    //val = value;
    return false;
  }
  bool GetValue(double &val)
  {
    //val = value;
    return false;
  }
  bool GetValue(string &str)
  {
    str = value;
    return true;
  }
};
