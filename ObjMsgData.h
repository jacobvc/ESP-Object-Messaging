#pragma once

#define isnan(n) ((n != n) ? 1 : 0)

using namespace std;

class ObjMsgData;
typedef shared_ptr<ObjMsgData> ObjMsgDataRef;

/*
 *      ___       _          ___        _
 *     |   \ __ _| |_ __ _  | __|_ _ __| |_ ___ _ _ _  _
 *     | |) / _` |  _/ _` | | _/ _` / _|  _/ _ \ '_| || |
 *     |___/\__,_|\__\__,_| |_|\__,_\__|\__\___/_|  \_, |
 *                                                  |__/
 */

 /// Factory to create ObjMsgDataRef from JSON data for registered data 'name'.
class ObjMsgDataFactory
{
  unordered_map<string, ObjMsgDataRef(*)(uint16_t, char const*)> dataClasses;

public:
  /// Register object creator function 'fn' to create object for endpoint 'name'
  /// @param origin - origin to register
  /// @param name - name to register
  /// @param fn - Create function
  /// @return bool registratin successful
  bool RegisterClass(uint16_t origin, string name, ObjMsgDataRef(*fn)(uint16_t, char const*));

  /// Create ObjMsgDataRef object for endpoint 'name'
  /// @param origin - origin of data
  /// @param name - registered name
  /// @return created object
  ObjMsgDataRef Create(uint16_t origin, char const* name);

  /// Create ObjMsgDataRef object and populate it based on 'json' content
  /// @param origin - origin of data
  /// @param json - JSON content
  /// @return 
  ObjMsgDataRef Deserialize(uint16_t origin, char const* json);
};

/*
 *       ___  _     _ __  __         ___       _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|
 *               |__/          |___/
 */

 /// ObjMsgData virtual base class
 /// 
 /// All data is exchanged using objects of the ObjMsgData base class
 /// 
 /// Identifies origin, endpoint name, and contains
 /// binary data with JSON serialization and deserialization.
 /// 
 /// Abstract base class for templatized ObjMsgDataT. Intended to always be
 /// instantiated using a ObjMsgDataRef (a std::shared_ptr)
class ObjMsgData
{
protected:
  /** ID of message originator */
  uint16_t origin;
  /** Endpoint name */
  string name;
  string TAG;

public:
  static ObjMsgDataFactory dataFactory;

  /// Constructor
  /// @param tag - TAG for log messages
  ObjMsgData(string tag) : TAG(tag) { }
  /// MANDATORY vitrual destructor
  virtual ~ObjMsgData() {}

  /// Origin ID accessor
  /// @return Origin ID
  uint16_t GetOrigin() { return origin; }

  /// Check is this ObjMsgData is from 'origin'
  /// @param origin: the origin to test
  /// @return boolean result
  bool IsFrom(int16_t origin) { return this->origin == origin; }

  /// ObjMsgData name accessor 
  /// @return the name
  string& GetName() { return name; }

  /// Create new ObjMsgDataRef by deseerializing 'json'
  /// @param origin - origin for ne object
  /// @param json: content to deserialize
  /// @return created ObjMsgDataRef
  static ObjMsgDataRef Deserialize(uint16_t origin, char const* json) {
    return dataFactory.Deserialize(origin, json);
  }

  /// Register object creator function 'fn' to create object for endpoint 'name'
  /// @param origin - origin to register
  /// @param name - name to register
  /// @param fn - Create function
  /// @return bool registratin successful
  static bool RegisterClass(uint16_t origin, string name, ObjMsgDataRef(*fn)(uint16_t, char const*))
  {
    return dataFactory.RegisterClass(origin, name, fn);
  }

  /// Populate value from the contents of 'json'
  /// @param json: JSON content to parse
  /// @return boolean success
  virtual bool DeserializeValue(cJSON* json) = 0;

  /// serialize this object into a JSON string
  /// @param json: out value
  /// @return boolean success
  virtual int Serialize(string& json) = 0;

  /// Get value as string
  /// @param str: out value
  /// @return true if can be represented as string
  virtual bool GetValue(string& str) = 0;
  /// Get value as integer
  /// @param val: out value 
  /// @return true if can be represented as integer
  virtual bool GetValue(int& val) = 0;
  ///  Get value as double
  /// @param val: out value 
  /// @return true if can be represented as double
  virtual bool GetValue(double& val) = 0;

};

/*
 *       ___  _     _ __  __         ___       _       _____
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ |_   _|
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` || |
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_||_|
 *               |__/          |___/
 */

 //#define RESOLVE_TYPES_FOR_LOGGING
 // Useful for development debugging at a cost of about
 //  2K bytes of flash and zero RAM
#ifdef RESOLVE_TYPES_FOR_LOGGING
#include <string_view>
#include <array>   // std::array
#include <utility> // std::index_sequence

template <std::size_t...Idxs>
constexpr auto substring_as_array(std::string_view str, std::index_sequence<Idxs...>)
{
  return std::array{ str[Idxs]..., '\n' };
}

template <typename T>
constexpr auto type_name_array()
{
#if defined(__clang__)
  constexpr auto prefix = std::string_view{ "[T = " };
  constexpr auto suffix = std::string_view{ "]" };
  constexpr auto function = std::string_view{ __PRETTY_FUNCTION__ };
#elif defined(__GNUC__)
  constexpr auto prefix = std::string_view{ "with T = " };
  constexpr auto suffix = std::string_view{ "]" };
  constexpr auto function = std::string_view{ __PRETTY_FUNCTION__ };
#elif defined(_MSC_VER)
  constexpr auto prefix = std::string_view{ "type_name_array<" };
  constexpr auto suffix = std::string_view{ ">(void)" };
  constexpr auto function = std::string_view{ __FUNCSIG__ };
#else
# error Unsupported compiler
#endif

  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);

  static_assert(start < end);

  constexpr auto name = function.substr(start, (end - start));
  return substring_as_array(name, std::make_index_sequence<name.size()>{});
}

template <typename T>
struct type_name_holder {
  static inline constexpr auto value = type_name_array<T>();
};

template <typename T>
constexpr auto type_name() -> std::string
{
  constexpr auto& value = type_name_holder<T>::value;
  return string{ std::string_view{value.data(), value.size() - 1} };
}
#endif

/// Templatized ObjMsgData class
/// @tparam T: data type
template <class T>
class ObjMsgDataT : public ObjMsgData
{
protected:
  /** The (template specific) binary value */
  T value;

  /// Constructor
  /// @param origin: data origin
  /// @param name: data object name
  ObjMsgDataT(uint16_t origin, char const* name)
#ifdef RESOLVE_TYPES_FOR_LOGGING
    : ObjMsgData("ObjMsgDataT<" + type_name<T>() + ">")
#else
    : ObjMsgData("ObjMsgDataT<T>")
#endif
  {
    this->origin = origin;
    this->name = name;
  }

public:
  bool GetRawValue(T& out) { out = value; return true; }

  /// Serialize to JSON string
  /// @param json: out value
  /// @return ESP_OK or error value
  int Serialize(string& json)
  {
    string val;
    GetValue(val);
    json = "{\"name\":\"" + name + "\", \"value\":" + val + "}";

    return 0;
  }

  /// MANDATORY vitrual destructor
  ~ObjMsgDataT() {}
};


/*
 *       ___  _     _ __  __         ___       _        ___     _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _|_ _|_ _| |_
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` || || ' \  _|
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|___|_||_\__|
 *               |__/          |___/
 */

 /// ObjMsgDataT<int>, integer scalar value
class ObjMsgDataInt : public ObjMsgDataT<int>
{
protected:
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  ObjMsgDataInt(uint16_t origin, char const* name, int value)
    : ObjMsgDataT<int>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataInt(%u, %s, %d) constructed", origin, name, value);
  }

  /// Destrructor
  ~ObjMsgDataInt()
  {
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataInt destructed");
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name, int value)
  {
    return std::make_shared<ObjMsgDataInt>(origin, name, value);
  }

  /// @brief 
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name)
  {
    return std::make_shared<ObjMsgDataInt>(origin, name, 0);
  }

  bool DeserializeValue(cJSON* json)
  {
    cJSON* valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj) {
      value = valueObj->valueint;
      return true;
    }
    return false;
  }

  bool GetValue(int& val)
  {
    val = value;
    return true;
  }
  bool GetValue(double& val)
  {
    val = value;
    return true;
  }
  bool GetValue(string& str)
  {
    char buffer[32];
    sprintf(buffer, "%d", value);
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

 // ObjMsgDataT<double>, floating point scalar value 
class ObjMsgDataFloat : public ObjMsgDataT<double>
{
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  ObjMsgDataFloat(uint16_t origin, char const* name, double value)
    : ObjMsgDataT<double>(origin, name)
  {
    this->value = value;
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataFloat(%u, %s, %f) constructed", origin, name, value);
  }

  /// Destructor
  ~ObjMsgDataFloat()
  {
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataFloat destructed");
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name)
  {
    return std::make_shared<ObjMsgDataFloat>(origin, name, 0);
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name, double value)
  {
    return std::make_shared<ObjMsgDataFloat>(origin, name, value);
  }

  bool DeserializeValue(cJSON* json)
  {
    cJSON* valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj) {
      double tmp = valueObj->valuedouble;
      if (!isnan(tmp)) {
        value = tmp;
        return true;
      }
    }
    return false;
  }

  bool GetValue(int& val)
  {
    val = value;
    return true;
  }
  bool GetValue(double& val)
  {
    val = value;
    return true;
  }
  bool GetValue(string& str)
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

 /** ObjMsgData<std::string>, string value
  *
  * Note that if this is constructed with asJson = True, the
  * (presumably json) string value is serialized without quotes.
  */
class ObjMsgDataString : public ObjMsgDataT<string>
{
protected:
  bool asJson;
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @param asJson: specify JSON object value rather than string
  ObjMsgDataString(uint16_t origin, char const* name, const char* value, bool asJson = false)
    : ObjMsgDataT<string>(origin, name), asJson(asJson)
  {
    this->value = value;
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataString(%u, %s, %u) constructed", origin, name, value);
  }
  /// Destructor
  ~ObjMsgDataString()
  {
    // ESP_LOGI(TAG.c_str(), "ObjMsgDataString destructed");
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @param asJson: specify JSON object value rather than string
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name, const char* value, bool asJson = false)
  {
    return std::make_shared<ObjMsgDataString>(origin, name, value, asJson);
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name)
  {
    return std::make_shared<ObjMsgDataString>(origin, name, "");
  }

  bool DeserializeValue(cJSON* json)
  {
    cJSON* valueObj = cJSON_GetObjectItem(json, "value");
    if (valueObj) {
      value = valueObj->valuestring;
      return true;
    }
    return false;
  }

  // Quoted value for strings
  int Serialize(string& json)
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
  bool GetValue(int& val)
  {
    //val = value;
    return false;
  }
  bool GetValue(double& val)
  {
    //val = value;
    return false;
  }
  bool GetValue(string& str)
  {
    str = value;
    return true;
  }
};

/***
 *       ___  _     _ __  __         ___       _           _
 *      / _ \| |__ (_)  \/  |_____ _|   \ __ _| |_ __ _ _ | |______ _ _
 *     | (_) | '_ \| | |\/| (_-< _` | |) / _` |  _/ _` | || (_-< _ \ ' \
 *      \___/|_.__// |_|  |_/__|__, |___/\__,_|\__\__,_|\__//__|___/_||_|
 *               |__/          |___/
 */

 /** ObjMsgData<cJSON *>, JSON value
  *
  */
class ObjMsgDataJson : public ObjMsgDataT<cJSON*>
{
public:
  /// Constructor
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  ObjMsgDataJson(uint16_t origin, char const* name, cJSON* value)
    : ObjMsgDataT<cJSON*>(origin, name)
  {
    this->value = value;
    //ESP_LOGW(TAG.c_str(), "ObjMsgDataJson(%u, %s, %p) constructed", origin, name, value);
  }
  /// Destructor
  ~ObjMsgDataJson()
  {
    //ESP_LOGW(TAG.c_str(), "ObjMsgDataJson %p destructed", value);
    if (value) {
      cJSON_Delete(value);
    }
    value = NULL;
  }

  /// Create object and return in ObjMsgDataRef
  /// @param origin: Data origin
  /// @param name: Data object name
  /// @param value: initial value
  /// @param asJson: specify JSON object value rather than string
  /// @return created object
  static ObjMsgDataRef Create(uint16_t origin, char const* name, cJSON* value = NULL)
  {
    return std::make_shared<ObjMsgDataJson>(origin, name, value);
  }

   static ObjMsgDataRef Create(uint16_t origin, char const* name)
  {
    return std::make_shared<ObjMsgDataJson>(origin, name, (cJSON *)NULL);
  }


  bool DeserializeValue(cJSON* json)
  {
    if (value) {
      cJSON_Delete(value);
    }
    value = json;
    if (json) {
      return true;
    }
    return false;
  }

  int Serialize(string& json)
  {
    json = "{\"name\":\"" + name + "\", \"value\":";

    json += cJSON_Print(value);
    json += " }";

    return 0;
  }

  // TODO try parsing and return true if value is a number
  bool GetValue(int& val)
  {
    return false;
  }
  bool GetValue(double& val)
  {
    return false;
  }
  bool GetValue(string& str)
  {
    if (value) {
      str = cJSON_Print(value);
      return true;
    }
    else {
      return false;
    }
  }
};

