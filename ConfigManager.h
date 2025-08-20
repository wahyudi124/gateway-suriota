#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_heap_caps.h>

class ConfigManager {
private:
  static const char* DEVICES_FILE;
  static const char* REGISTERS_FILE;
  
  // Cache for devices and registers
  DynamicJsonDocument* devicesCache;
  DynamicJsonDocument* registersCache;
  bool devicesCacheValid;
  bool registersCacheValid;
  
  String generateId(const String& prefix);
  bool saveJson(const String& filename, const JsonDocument& doc);
  bool loadJson(const String& filename, JsonDocument& doc);
  void invalidateDevicesCache();
  void invalidateRegistersCache();
  bool loadDevicesCache();
  bool loadRegistersCache();

public:
  ConfigManager();
  ~ConfigManager();
  
  bool begin();
  
  // Device operations
  String createDevice(JsonObjectConst config);
  bool readDevice(const String& deviceId, JsonObject& result);
  bool deleteDevice(const String& deviceId);
  void listDevices(JsonArray& devices);
  void getDevicesSummary(JsonArray& summary);
  
  // Clear all configurations
  void clearAllConfigurations();
  
  // Cache management
  void refreshCache();
  
  // Register operations
  String createRegister(const String& deviceId, JsonObjectConst config);
  bool listRegisters(const String& deviceId, JsonArray& registers);
  bool getRegistersSummary(const String& deviceId, JsonArray& summary);
  bool deleteRegister(const String& deviceId, const String& registerId);
};

#endif