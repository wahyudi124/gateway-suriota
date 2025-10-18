#include "ConfigManager.h"
#include <esp_heap_caps.h>
#include <new>
#include <vector>

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
  
  // Force load devices cache
  devicesCacheValid = false;
  bool devicesLoaded = loadDevicesCache();
  Serial.printf("Devices cache load result: %s\n", devicesLoaded ? "SUCCESS" : "FAILED");
  
  // Force load registers cache
  registersCacheValid = false;
  bool registersLoaded = loadRegistersCache();
  Serial.printf("Registers cache load result: %s\n", registersLoaded ? "SUCCESS" : "FAILED");
  
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
  
  Serial.printf("[DEBUG] Looking for device ID: '%s'\n", deviceId.c_str());
  Serial.printf("[DEBUG] Device ID length: %d\n", deviceId.length());
  
  if (devicesCache->containsKey(deviceId)) {
    JsonObject device = (*devicesCache)[deviceId];
    for (JsonPair kv : device) {
      result[kv.key()] = kv.value();
    }
    Serial.printf("Device %s read from cache\n", deviceId.c_str());
    return true;
  }
  
  // Debug: Show all available keys
  Serial.printf("Device %s not found in cache. Available devices:\n", deviceId.c_str());
  for (JsonPair kv : devicesCache->as<JsonObject>()) {
    Serial.printf("  - '%s' (length: %d)\n", kv.key().c_str(), String(kv.key().c_str()).length());
  }
  
  return false;
}

bool ConfigManager::updateDevice(const String& deviceId, JsonObjectConst config) {
  if (!loadDevicesCache()) return false;
  
  if (!devicesCache->containsKey(deviceId)) {
    Serial.printf("Device %s not found for update\n", deviceId.c_str());
    return false;
  }
  
  JsonObject device = (*devicesCache)[deviceId];
  
  // Update device configuration while preserving device_id and registers
  JsonArray existingRegisters = device["registers"];
  
  // Update all config fields
  for (JsonPairConst kv : config) {
    device[kv.key()] = kv.value();
  }
  
  // Ensure device_id and registers are preserved
  device["device_id"] = deviceId;
  if (!device.containsKey("registers")) {
    device["registers"] = existingRegisters;
  }
  
  // Save to file and keep cache valid
  if (saveJson(DEVICES_FILE, *devicesCache)) {
    Serial.printf("Device %s updated successfully\n", deviceId.c_str());
    return true;
  }
  
  invalidateDevicesCache();
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
  
  JsonObject devicesObj = devicesCache->as<JsonObject>();
  int count = 0;
  
  Serial.printf("[DEBUG] Cache size: %d devices\n", devicesObj.size());
  
  for (JsonPair kv : devicesObj) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);
    
    Serial.printf("[DEBUG] Raw key: '%s', String ID: '%s' (len: %d)\n", 
                  keyPtr, deviceId.c_str(), deviceId.length());
    
    // Validate device ID before adding
    if (deviceId.length() > 0 && deviceId != "{}" && deviceId.indexOf('{') == -1) {
      devices.add(deviceId);
      count++;
      Serial.printf("[DEBUG] Added valid device ID: '%s'\n", deviceId.c_str());
    } else {
      Serial.printf("[DEBUG] Skipped invalid device ID: '%s'\n", deviceId.c_str());
    }
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

bool ConfigManager::updateRegister(const String& deviceId, const String& registerId, JsonObjectConst config) {
  if (!loadDevicesCache()) return false;
  
  if (!devicesCache->containsKey(deviceId)) {
    Serial.printf("Device %s not found for register update\n", deviceId.c_str());
    return false;
  }
  
  JsonObject device = (*devicesCache)[deviceId];
  if (!device.containsKey("registers")) {
    Serial.printf("No registers found for device %s\n", deviceId.c_str());
    return false;
  }
  
  JsonArray registers = device["registers"];
  for (JsonVariant regVar : registers) {
    JsonObject reg = regVar.as<JsonObject>();
    if (reg["register_id"] == registerId) {
      // Update register configuration while preserving register_id
      for (JsonPairConst kv : config) {
        reg[kv.key()] = kv.value();
      }
      reg["register_id"] = registerId; // Ensure register_id is preserved
      
      // Save to file and keep cache valid
      if (saveJson(DEVICES_FILE, *devicesCache)) {
        Serial.printf("Register %s updated successfully\n", registerId.c_str());
        return true;
      }
      
      invalidateDevicesCache();
      return false;
    }
  }
  
  Serial.printf("Register %s not found in device %s\n", registerId.c_str(), deviceId.c_str());
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
  if (devicesCacheValid) {
    Serial.printf("[CACHE] Devices cache already valid: %d devices\n", devicesCache->as<JsonObject>().size());
    return true;
  }
  
  Serial.println("[CACHE] Loading devices cache from file...");
  
  Serial.println("Loading devices cache...");
  
  // Check if file exists and has content
  if (!SPIFFS.exists(DEVICES_FILE)) {
    Serial.println("Devices file does not exist");
    return false;
  }
  
  File file = SPIFFS.open(DEVICES_FILE, "r");
  if (!file) {
    Serial.println("Failed to open devices file");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.printf("Devices file size: %d bytes\n", fileSize);
  
  if (fileSize == 0) {
    Serial.println("Devices file is empty");
    file.close();
    return false;
  }
  
  file.close();
  
  if (loadJson(DEVICES_FILE, *devicesCache)) {
    devicesCacheValid = true;
    JsonObject devicesObj = devicesCache->as<JsonObject>();
    Serial.printf("Devices cache loaded: %d devices\n", devicesObj.size());
    
    // Debug: Print device IDs and their content
    for (JsonPair kv : devicesObj) {
      const char* keyPtr = kv.key().c_str();
      Serial.printf("Found device - Key: '%s' (len: %d)\n", keyPtr, strlen(keyPtr));
      
      // Print first few characters in hex to check for hidden characters
      Serial.print("Key hex: ");
      for (int i = 0; i < min(10, (int)strlen(keyPtr)); i++) {
        Serial.printf("%02X ", (unsigned char)keyPtr[i]);
      }
      Serial.println();
      
      // Check device content
      JsonObject deviceObj = kv.value().as<JsonObject>();
      if (deviceObj.containsKey("device_name")) {
        Serial.printf("  Device name: %s\n", deviceObj["device_name"].as<String>().c_str());
      }
    }
    
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
  Serial.println("[CACHE] Forcing cache refresh...");
  
  // Force invalidate
  devicesCacheValid = false;
  registersCacheValid = false;
  
  // Clear cache content
  devicesCache->clear();
  registersCache->clear();
  
  // Reload from files
  bool devicesLoaded = loadDevicesCache();
  bool registersLoaded = loadRegistersCache();
  
  Serial.printf("[CACHE] Refresh complete - Devices: %s, Registers: %s\n", 
                devicesLoaded ? "OK" : "FAIL", 
                registersLoaded ? "OK" : "FAIL");
}

void ConfigManager::debugDevicesFile() {
  Serial.println("=== DEBUG DEVICES FILE ===");
  
  if (!SPIFFS.exists(DEVICES_FILE)) {
    Serial.println("Devices file does not exist");
    return;
  }
  
  File file = SPIFFS.open(DEVICES_FILE, "r");
  if (!file) {
    Serial.println("Failed to open devices file");
    return;
  }
  
  Serial.printf("File size: %d bytes\n", file.size());
  Serial.println("File content:");
  
  while (file.available()) {
    Serial.write(file.read());
  }
  Serial.println();
  
  file.close();
  Serial.println("=== END DEBUG ===");
}

void ConfigManager::fixCorruptDeviceIds() {
  Serial.println("=== FIXING CORRUPT DEVICE IDS ===");
  
  DynamicJsonDocument originalDoc(8192);
  if (!loadJson(DEVICES_FILE, originalDoc)) {
    Serial.println("Failed to load devices file for fixing");
    return;
  }
  
  DynamicJsonDocument fixedDoc(8192);
  JsonObject fixedDevices = fixedDoc.to<JsonObject>();
  
  bool foundCorruption = false;
  
  for (JsonPair kv : originalDoc.as<JsonObject>()) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);
    
    // Check if device ID is corrupt
    if (deviceId.isEmpty() || deviceId == "{}" || deviceId.indexOf('{') != -1 || deviceId.length() < 3) {
      Serial.printf("Found corrupt device ID: '%s' - generating new ID\n", deviceId.c_str());
      
      // Generate new device ID
      String newDeviceId = generateId("D");
      JsonObject deviceObj = kv.value().as<JsonObject>();
      deviceObj["device_id"] = newDeviceId;
      
      fixedDevices[newDeviceId] = deviceObj;
      foundCorruption = true;
      
      Serial.printf("Replaced with new ID: %s\n", newDeviceId.c_str());
    } else {
      // Keep valid device ID
      fixedDevices[deviceId] = kv.value();
      Serial.printf("Kept valid device ID: %s\n", deviceId.c_str());
    }
  }
  
  if (foundCorruption) {
    Serial.println("Saving fixed devices file...");
    if (saveJson(DEVICES_FILE, fixedDoc)) {
      Serial.println("Fixed devices file saved successfully");
      // Force invalidate cache to reload fixed data
      invalidateDevicesCache();
      devicesCacheValid = false;
    } else {
      Serial.println("Failed to save fixed devices file");
    }
  } else {
    Serial.println("No corruption found in device IDs");
  }
  
  // Always invalidate cache after this operation
  invalidateDevicesCache();
  devicesCacheValid = false;
  
  Serial.println("=== END FIXING ===");
}

void ConfigManager::removeCorruptKeys() {
  Serial.println("=== REMOVING CORRUPT KEYS ===");
  
  // Force load current cache
  devicesCacheValid = false;
  if (!loadDevicesCache()) {
    Serial.println("Failed to load cache for corrupt key removal");
    return;
  }
  
  JsonObject devicesObj = devicesCache->as<JsonObject>();
  
  // Find and remove corrupt keys
  std::vector<String> keysToRemove;
  
  for (JsonPair kv : devicesObj) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);
    
    if (deviceId.isEmpty() || deviceId == "{}" || deviceId.indexOf('{') != -1 || deviceId.length() < 3) {
      keysToRemove.push_back(deviceId);
      Serial.printf("Marking corrupt key for removal: '%s'\n", deviceId.c_str());
    }
  }
  
  // Remove corrupt keys
  for (const String& key : keysToRemove) {
    devicesCache->remove(key);
    Serial.printf("Removed corrupt key: '%s'\n", key.c_str());
  }
  
  if (keysToRemove.size() > 0) {
    // Save cleaned cache
    if (saveJson(DEVICES_FILE, *devicesCache)) {
      Serial.printf("Removed %d corrupt keys and saved file\n", keysToRemove.size());
    } else {
      Serial.println("Failed to save cleaned devices file");
    }
  } else {
    Serial.println("No corrupt keys found to remove");
  }
  
  Serial.println("=== END REMOVING ===");
}

void ConfigManager::createTestDeviceIfEmpty() {
  Serial.println("=== CREATING TEST DEVICE IF EMPTY ===");
  
  // Force reload cache
  devicesCacheValid = false;
  if (!loadDevicesCache()) {
    Serial.println("Failed to load cache, creating empty cache");
    devicesCache->clear();
    devicesCache->to<JsonObject>();
  }
  
  JsonObject devicesObj = devicesCache->as<JsonObject>();
  
  // Count valid devices
  int validDeviceCount = 0;
  for (JsonPair kv : devicesObj) {
    const char* keyPtr = kv.key().c_str();
    String deviceId = String(keyPtr);
    
    if (deviceId.length() > 2 && deviceId != "{}" && deviceId.indexOf('{') == -1) {
      validDeviceCount++;
    }
  }
  
  Serial.printf("Found %d valid devices\n", validDeviceCount);
  
  if (validDeviceCount == 0) {
    Serial.println("No valid devices found, creating test device...");
    
    // Create test device
    String deviceId = generateId("D");
    JsonObject device = devicesCache->createNestedObject(deviceId);
    
    device["device_id"] = deviceId;
    device["device_name"] = "Test RTU Device";
    device["protocol"] = "RTU";
    device["serial_port"] = 1;
    device["baud_rate"] = 9600;
    device["parity"] = "None";
    device["data_bits"] = 8;
    device["stop_bits"] = 1;
    device["slave_id"] = 1;
    device["timeout"] = 1000;
    device["retry_count"] = 3;
    device["refresh_rate_ms"] = 5000;
    
    // Create test register
    JsonArray registers = device.createNestedArray("registers");
    JsonObject reg = registers.createNestedObject();
    
    String registerId = generateId("R");
    reg["register_id"] = registerId;
    reg["address"] = 40001;
    reg["register_name"] = "SUHU";
    reg["type"] = "Holding Register";
    reg["function_code"] = 3;
    reg["data_type"] = "float32";
    reg["description"] = "Temperature Sensor";
    reg["refresh_rate_ms"] = 5000;
    
    // Save to file
    if (saveJson(DEVICES_FILE, *devicesCache)) {
      Serial.printf("Created test device %s with register %s\n", deviceId.c_str(), registerId.c_str());
      devicesCacheValid = true;
    } else {
      Serial.println("Failed to save test device");
    }
  } else {
    Serial.println("Valid devices exist, no need to create test device");
  }
  
  Serial.println("=== END CREATING TEST DEVICE ===");
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