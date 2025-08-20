#include "ConfigManager.h"
#include <esp_heap_caps.h>
#include <new>

const char* ConfigManager::DEVICES_FILE = "/devices.json";
const char* ConfigManager::REGISTERS_FILE = "/registers.json";

ConfigManager::ConfigManager() : devicesCache(nullptr), registersCache(nullptr), 
                                 devicesCacheValid(false), registersCacheValid(false) {
  // Initialize cache in PSRAM
  devicesCache = (DynamicJsonDocument*)heap_caps_malloc(sizeof(DynamicJsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (devicesCache) {
    new(devicesCache) DynamicJsonDocument(8192);
  } else {
    devicesCache = new DynamicJsonDocument(4096);
  }
  
  registersCache = (DynamicJsonDocument*)heap_caps_malloc(sizeof(DynamicJsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (registersCache) {
    new(registersCache) DynamicJsonDocument(16384);
  } else {
    registersCache = new DynamicJsonDocument(8192);
  }
}

ConfigManager::~ConfigManager() {
  if (devicesCache) {
    devicesCache->~DynamicJsonDocument();
    heap_caps_free(devicesCache);
  }
  if (registersCache) {
    registersCache->~DynamicJsonDocument();
    heap_caps_free(registersCache);
  }
}

bool ConfigManager::begin() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return false;
  }
  
  // Initialize empty files if they don't exist
  if (!SPIFFS.exists(DEVICES_FILE)) {
    DynamicJsonDocument doc(64);
    doc.to<JsonObject>();
    saveJson(DEVICES_FILE, doc);
  }
  
  if (!SPIFFS.exists(REGISTERS_FILE)) {
    DynamicJsonDocument doc(64);
    doc.to<JsonObject>();
    saveJson(REGISTERS_FILE, doc);
  }
  
  // Load cache on startup
  Serial.println("Loading configuration cache...");
  loadDevicesCache();
  loadRegistersCache();
  
  Serial.println("ConfigManager initialized with cache loaded");
  return true;
}

String ConfigManager::generateId(const String& prefix) {
  return prefix + String(random(100000, 999999), HEX).substring(0, 6);
}

bool ConfigManager::saveJson(const String& filename, const JsonDocument& doc) {
  File file = SPIFFS.open(filename, "w");
  if (!file) return false;
  
  serializeJson(doc, file);
  file.close();
  return true;
}

bool ConfigManager::loadJson(const String& filename, JsonDocument& doc) {
  File file = SPIFFS.open(filename, "r");
  if (!file) return false;
  
  // Read file to PSRAM buffer for large files
  size_t fileSize = file.size();
  char* buffer = (char*)heap_caps_malloc(fileSize + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buffer) {
    // Fallback to direct parsing
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    return error == DeserializationError::Ok;
  }
  
  file.readBytes(buffer, fileSize);
  buffer[fileSize] = '\0';
  file.close();
  
  DeserializationError error = deserializeJson(doc, buffer);
  heap_caps_free(buffer);
  
  return error == DeserializationError::Ok;
}

String ConfigManager::createDevice(JsonObjectConst config) {
  if (!loadDevicesCache()) return "";
  
  String deviceId = generateId("D");
  JsonObject device = devicesCache->createNestedObject(deviceId);
  
  // Copy config
  for (JsonPairConst kv : config) {
    device[kv.key()] = kv.value();
  }
  device["device_id"] = deviceId;
  JsonArray registers = device.createNestedArray("registers");
  Serial.printf("Created device %s with empty registers array\n", deviceId.c_str());
  
  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    Serial.printf("Device %s created and cache updated\n", deviceId.c_str());
    return deviceId;
  }
  
  invalidateDevicesCache();
  return "";
}

bool ConfigManager::readDevice(const String& deviceId, JsonObject& result) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for readDevice");
    return false;
  }
  
  if (devicesCache->containsKey(deviceId)) {
    JsonObject device = (*devicesCache)[deviceId];
    for (JsonPair kv : device) {
      result[kv.key()] = kv.value();
    }
    Serial.printf("Device %s read from cache\n", deviceId.c_str());
    return true;
  }
  Serial.printf("Device %s not found in cache\n", deviceId.c_str());
  return false;
}

bool ConfigManager::deleteDevice(const String& deviceId) {
  if (!loadDevicesCache()) return false;
  
  if (devicesCache->containsKey(deviceId)) {
    devicesCache->remove(deviceId);
    if (saveJson(DEVICES_FILE, *devicesCache)) {
      return true;
    }
    invalidateDevicesCache();
  }
  return false;
}

void ConfigManager::listDevices(JsonArray& devices) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache for listDevices");
    return;
  }
  
  int count = 0;
  for (JsonPair kv : devicesCache->as<JsonObject>()) {
    devices.add(kv.key().c_str());
    count++;
  }
  Serial.printf("Listed %d devices from cache\n", count);
}

void ConfigManager::getDevicesSummary(JsonArray& summary) {
  DynamicJsonDocument devices(4096);
  if (!loadJson(DEVICES_FILE, devices)) return;
  
  for (JsonPair kv : devices.as<JsonObject>()) {
    JsonObject device = kv.value();
    JsonObject deviceSummary = summary.createNestedObject();
    
    deviceSummary["device_id"] = kv.key();
    deviceSummary["device_name"] = device["device_name"];
    deviceSummary["protocol"] = device["protocol"];
    deviceSummary["register_count"] = device["registers"].size();
  }
}

String ConfigManager::createRegister(const String& deviceId, JsonObjectConst config) {
  if (!loadDevicesCache()) {
    Serial.println("Failed to load devices cache");
    return "";
  }
  
  if (!devicesCache->containsKey(deviceId)) {
    Serial.printf("Device %s not found in cache\n", deviceId.c_str());
    return "";
  }
  
  String registerId = generateId("R");
  JsonObject device = (*devicesCache)[deviceId];
  
  // Ensure registers array exists
  if (!device.containsKey("registers")) {
    device["registers"] = JsonArray();
    Serial.println("Created registers array for device");
  }
  
  JsonArray registers = device["registers"];
  Serial.printf("Registers array size before: %d\n", registers.size());
  
  JsonObject newRegister = registers.createNestedObject();
  for (JsonPairConst kv : config) {
    newRegister[kv.key()] = kv.value();
  }
  newRegister["register_id"] = registerId;
  
  Serial.printf("Registers array size after: %d\n", registers.size());
  Serial.printf("Created register %s for device %s\n", registerId.c_str(), deviceId.c_str());
  
  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    Serial.println("Successfully saved devices file and updated cache");
    return registerId;
  } else {
    Serial.println("Failed to save devices file");
    invalidateDevicesCache();
  }
  return "";
}

bool ConfigManager::listRegisters(const String& deviceId, JsonArray& registers) {
  DynamicJsonDocument devices(8192); // Increased size for registers
  if (!loadJson(DEVICES_FILE, devices)) return false;
  
  if (devices.containsKey(deviceId)) {
    JsonArray deviceRegisters = devices[deviceId]["registers"];
    Serial.printf("Device %s has %d registers in storage\n", deviceId.c_str(), deviceRegisters.size());
    for (JsonVariant reg : deviceRegisters) {
      registers.add(reg);
    }
    return true;
  }
  return false;
}

bool ConfigManager::getRegistersSummary(const String& deviceId, JsonArray& summary) {
  DynamicJsonDocument devices(4096);
  if (!loadJson(DEVICES_FILE, devices)) return false;
  
  if (devices.containsKey(deviceId)) {
    JsonArray registers = devices[deviceId]["registers"];
    for (JsonVariant reg : registers) {
      JsonObject regSummary = summary.createNestedObject();
      regSummary["register_id"] = reg["register_id"];
      regSummary["register_name"] = reg["register_name"];
      regSummary["address"] = reg["address"];
      regSummary["data_type"] = reg["data_type"];
      regSummary["description"] = reg["description"];
    }
    return true;
  }
  return false;
}

bool ConfigManager::deleteRegister(const String& deviceId, const String& registerId) {
  DynamicJsonDocument devices(4096);
  if (!loadJson(DEVICES_FILE, devices)) return false;
  
  if (devices.containsKey(deviceId)) {
    JsonArray registers = devices[deviceId]["registers"];
    for (int i = 0; i < registers.size(); i++) {
      if (registers[i]["register_id"] == registerId) {
        registers.remove(i);
        return saveJson(DEVICES_FILE, devices);
      }
    }
  }
  return false;
}

bool ConfigManager::loadDevicesCache() {
  if (devicesCacheValid) return true;
  
  Serial.println("Loading devices cache...");
  if (loadJson(DEVICES_FILE, *devicesCache)) {
    devicesCacheValid = true;
    Serial.printf("Devices cache loaded: %d devices\n", devicesCache->as<JsonObject>().size());
    return true;
  }
  Serial.println("Failed to load devices cache");
  return false;
}

bool ConfigManager::loadRegistersCache() {
  if (registersCacheValid) return true;
  
  if (loadJson(REGISTERS_FILE, *registersCache)) {
    registersCacheValid = true;
    return true;
  }
  return false;
}

void ConfigManager::invalidateDevicesCache() {
  devicesCacheValid = false;
}

void ConfigManager::invalidateRegistersCache() {
  registersCacheValid = false;
}

void ConfigManager::refreshCache() {
  invalidateDevicesCache();
  invalidateRegistersCache();
  loadDevicesCache();
  loadRegistersCache();
}

void ConfigManager::clearAllConfigurations() {
  Serial.println("Clearing all device and register configurations...");
  DynamicJsonDocument emptyDoc(64);
  emptyDoc.to<JsonObject>();
  saveJson(DEVICES_FILE, emptyDoc);
  saveJson(REGISTERS_FILE, emptyDoc);
  invalidateDevicesCache();
  invalidateRegistersCache();
  Serial.println("All configurations cleared");
}