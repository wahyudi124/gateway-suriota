#include "ModbusTcpService.h"
#include "QueueManager.h"
#include "CRUDHandler.h"
#include "RTCManager.h"

extern CRUDHandler* crudHandler;

uint16_t ModbusTcpService::transactionCounter = 1;

ModbusTcpService::ModbusTcpService(ConfigManager* config, EthernetManager* ethernet) 
  : configManager(config), ethernetManager(ethernet), running(false), taskHandle(nullptr) {}

bool ModbusTcpService::init() {
  Serial.println("Initializing custom Modbus TCP service...");
  
  if (!configManager) {
    Serial.println("ConfigManager is null");
    return false;
  }
  
  if (!ethernetManager) {
    Serial.println("EthernetManager is null");
    return false;
  }
  
  Serial.printf("Ethernet available: %s\n", ethernetManager->isAvailable() ? "YES" : "NO");
  Serial.println("Custom Modbus TCP service initialized successfully");
  return true;
}

void ModbusTcpService::start() {
  Serial.println("Starting custom Modbus TCP service...");
  
  if (running) {
    Serial.println("Service already running");
    return;
  }
  
  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
    readTcpDevicesTask,
    "MODBUS_TCP_TASK",
    8192,
    this,
    2,
    &taskHandle,
    1
  );
  
  if (result == pdPASS) {
    Serial.println("Custom Modbus TCP service started successfully");
  } else {
    Serial.println("Failed to create Modbus TCP task");
    running = false;
    taskHandle = nullptr;
  }
}

void ModbusTcpService::stop() {
  running = false;
  if (taskHandle) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow task to exit gracefully
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  Serial.println("Custom Modbus TCP service stopped");
}

void ModbusTcpService::readTcpDevicesTask(void* parameter) {
  ModbusTcpService* service = static_cast<ModbusTcpService*>(parameter);
  service->readTcpDevicesLoop();
}

void ModbusTcpService::readTcpDevicesLoop() {
  DeviceTimer deviceTimers[10]; // Support up to 10 devices
  int timerCount = 0;
  
  // Custom Modbus TCP loop started
  
  while (running) {
    // Check Ethernet availability only
    if (!ethernetManager || !ethernetManager->isAvailable()) {
      vTaskDelay(pdMS_TO_TICKS(10000));
      continue;
    }
    
    // Get TCP devices
    DynamicJsonDocument devicesDoc(2048);
    JsonArray devices = devicesDoc.to<JsonArray>();
    configManager->listDevices(devices);
    
    unsigned long currentTime = millis();
    
    for (JsonVariant deviceVar : devices) {
      if (!running) break; // Exit if stopped
      
      String deviceId = deviceVar.as<String>();
      
      DynamicJsonDocument deviceDoc(2048);
      JsonObject deviceObj = deviceDoc.to<JsonObject>();
      if (configManager->readDevice(deviceId, deviceObj)) {
        String protocol = deviceObj["protocol"] | "";
        
        if (protocol == "TCP") {
          int refreshRate = deviceObj["refresh_rate_ms"] | 5000;
          
          // Find or create timer for this device
          int timerIndex = -1;
          for (int i = 0; i < timerCount; i++) {
            if (deviceTimers[i].deviceId == deviceId) {
              timerIndex = i;
              break;
            }
          }
          
          if (timerIndex == -1 && timerCount < 10) {
            timerIndex = timerCount++;
            deviceTimers[timerIndex].deviceId = deviceId;
            deviceTimers[timerIndex].lastRead = 0;
          }
          
          if (timerIndex >= 0) {
            if (currentTime - deviceTimers[timerIndex].lastRead >= refreshRate) {
              readTcpDeviceData(deviceObj);
              deviceTimers[timerIndex].lastRead = currentTime;
            }
          }
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void ModbusTcpService::readTcpDeviceData(const JsonObject& deviceConfig) {
  String deviceId = deviceConfig["device_id"] | "UNKNOWN";
  String ip = deviceConfig["ip"] | "";
  int port = deviceConfig["port"] | 502;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  JsonArray registers = deviceConfig["registers"];
  
  if (ip.isEmpty() || registers.size() == 0) {
    return;
  }
  
  Serial.printf("Reading Ethernet device %s at %s:%d\n", deviceId.c_str(), ip.c_str(), port);
  
  for (JsonVariant regVar : registers) {
    if (!running) break;
    
    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    String registerName = reg["register_name"] | "Unknown";
    String dataType = reg["data_type"] | "int16";
    
    if (functionCode == 1 || functionCode == 2) {
      // Read coils/discrete inputs
      bool result = false;
      if (readModbusCoil(ip, port, slaveId, address, &result)) {
        float value = result ? 1.0 : 0.0;
        storeRegisterValue(deviceId, reg, value);
        Serial.printf("bool = %s\n", result ? "true" : "false");
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    } else {
      // Read registers
      int registerCount = 1;
      
      // Determine register count based on data type
      if (dataType.startsWith("INT32") || dataType.startsWith("UINT32") || dataType.startsWith("FLOAT32")) {
        registerCount = 2;
      } else if (dataType.startsWith("INT64") || dataType.startsWith("UINT64") || dataType.startsWith("DOUBLE64")) {
        registerCount = 4;
      }
      
      uint16_t results[4];
      if (readModbusRegisters(ip, port, slaveId, functionCode, address, registerCount, results)) {
        if (registerCount == 1) {
          float value = processRegisterValue(reg, results[0]);
          storeRegisterValue(deviceId, reg, value);
          if (dataType == "int16") {
            Serial.printf("int16 = %d\n", (int16_t)value);
          } else if (dataType == "uint16") {
            Serial.printf("uint16 = %u\n", (uint16_t)value);
          } else {
            Serial.printf("%s = %.2f\n", dataType.c_str(), value);
          }
        } else if (registerCount == 2 && (dataType.startsWith("INT32") || dataType.startsWith("UINT32"))) {
          uint32_t combined;
          if (dataType.endsWith("_BE")) {
            combined = ((uint32_t)results[0] << 16) | results[1];
          } else if (dataType.endsWith("_LE")) {
            combined = ((uint32_t)results[1] << 16) | results[0];
          } else if (dataType.endsWith("_LE_BS")) {
            uint16_t word1 = ((results[0] & 0xFF) << 8) | ((results[0] & 0xFF00) >> 8);
            uint16_t word2 = ((results[1] & 0xFF) << 8) | ((results[1] & 0xFF00) >> 8);
            combined = ((uint32_t)word2 << 16) | word1;
          } else if (dataType.endsWith("_BE_BS")) {
            uint16_t word1 = ((results[0] & 0xFF) << 8) | ((results[0] & 0xFF00) >> 8);
            uint16_t word2 = ((results[1] & 0xFF) << 8) | ((results[1] & 0xFF00) >> 8);
            combined = ((uint32_t)word1 << 16) | word2;
          } else {
            combined = ((uint32_t)results[0] << 16) | results[1]; // Default BE
          }
          
          if (dataType.startsWith("INT32")) {
            int32_t signedValue = (int32_t)combined;
            storeInt32RegisterValue(deviceId, reg, signedValue);
            Serial.printf("INT32_%s = %d\n", dataType.substring(5).c_str(), signedValue);
          } else {
            storeUint32RegisterValue(deviceId, reg, combined);
            Serial.printf("UINT32_%s = %u\n", dataType.substring(6).c_str(), combined);
          }
        } else if (registerCount == 2 && dataType.startsWith("FLOAT32")) {
          uint32_t combined;
          if (dataType.endsWith("_BE")) {
            combined = ((uint32_t)results[0] << 16) | results[1];
          } else if (dataType.endsWith("_LE")) {
            combined = ((uint32_t)results[1] << 16) | results[0];
          } else if (dataType.endsWith("_LE_BS")) {
            uint16_t word1 = ((results[0] & 0xFF) << 8) | ((results[0] & 0xFF00) >> 8);
            uint16_t word2 = ((results[1] & 0xFF) << 8) | ((results[1] & 0xFF00) >> 8);
            combined = ((uint32_t)word2 << 16) | word1;
          } else if (dataType.endsWith("_BE_BS")) {
            uint16_t word1 = ((results[0] & 0xFF) << 8) | ((results[0] & 0xFF00) >> 8);
            uint16_t word2 = ((results[1] & 0xFF) << 8) | ((results[1] & 0xFF00) >> 8);
            combined = ((uint32_t)word1 << 16) | word2;
          } else {
            combined = ((uint32_t)results[0] << 16) | results[1]; // Default BE
          }
          
          float floatValue = *(float*)&combined;
          storeRegisterValue(deviceId, reg, floatValue);
          Serial.printf("FLOAT32_%s = %.6f\n", dataType.substring(7).c_str(), floatValue);
        } else {
          float value = processMultiRegisterValue(reg, results, registerCount);
          storeRegisterValue(deviceId, reg, value);
          Serial.printf("%s = %.2f\n", dataType.c_str(), value);
        }
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool ModbusTcpService::readModbusRegister(const String& ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, uint16_t* result) {
  EthernetClient client;
  
  if (!client.connect(ip.c_str(), port)) {
    return false;
  }
  
  // Build Modbus TCP request
  uint8_t request[12];
  uint16_t transId = transactionCounter++;
  buildModbusRequest(request, transId, slaveId, functionCode, address, 1);
  
  // Send request
  client.write(request, 12);
  
  // Wait for response with timeout
  unsigned long timeout = millis() + 5000;
  while (client.available() < 9 && millis() < timeout) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  if (client.available() < 9) {
    client.stop();
    return false;
  }
  
  // Read response
  uint8_t response[256];
  int bytesRead = client.readBytes(response, client.available());
  client.stop();
  
  return parseModbusResponse(response, bytesRead, functionCode, result);
}

bool ModbusTcpService::readModbusRegisters(const String& ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, int count, uint16_t* results) {
  EthernetClient client;
  
  if (!client.connect(ip.c_str(), port)) {
    return false;
  }
  
  // Build Modbus TCP request
  uint8_t request[12];
  uint16_t transId = transactionCounter++;
  buildModbusRequest(request, transId, slaveId, functionCode, address, count);
  
  // Send request
  client.write(request, 12);
  
  // Wait for response with timeout
  unsigned long timeout = millis() + 5000;
  int expectedBytes = 9 + (count * 2);
  while (client.available() < expectedBytes && millis() < timeout) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  if (client.available() < expectedBytes) {
    client.stop();
    return false;
  }
  
  // Read response
  uint8_t response[256];
  int bytesRead = client.readBytes(response, client.available());
  client.stop();
  
  return parseMultiModbusResponse(response, bytesRead, functionCode, count, results);
}

bool ModbusTcpService::readModbusCoil(const String& ip, int port, uint8_t slaveId, uint16_t address, bool* result) {
  EthernetClient client;
  
  if (!client.connect(ip.c_str(), port)) {
    return false;
  }
  
  // Build Modbus TCP request for coil
  uint8_t request[12];
  uint16_t transId = transactionCounter++;
  buildModbusRequest(request, transId, slaveId, 1, address, 1); // Function code 1 for coils
  
  // Send request
  client.write(request, 12);
  
  // Wait for response with timeout
  unsigned long timeout = millis() + 5000;
  while (client.available() < 9 && millis() < timeout) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  if (client.available() < 9) {
    client.stop();
    return false;
  }
  
  // Read response
  uint8_t response[256];
  int bytesRead = client.readBytes(response, client.available());
  client.stop();
  
  uint16_t dummy;
  return parseModbusResponse(response, bytesRead, 1, &dummy, result);
}

void ModbusTcpService::buildModbusRequest(uint8_t* buffer, uint16_t transId, uint8_t unitId, uint8_t funcCode, uint16_t addr, uint16_t qty) {
  // Modbus TCP header
  buffer[0] = (transId >> 8) & 0xFF;  // Transaction ID high
  buffer[1] = transId & 0xFF;         // Transaction ID low
  buffer[2] = 0x00;                   // Protocol ID high
  buffer[3] = 0x00;                   // Protocol ID low
  buffer[4] = 0x00;                   // Length high
  buffer[5] = 0x06;                   // Length low (6 bytes following)
  buffer[6] = unitId;                 // Unit ID
  
  // Modbus PDU
  buffer[7] = funcCode;               // Function code
  buffer[8] = (addr >> 8) & 0xFF;     // Start address high
  buffer[9] = addr & 0xFF;            // Start address low
  buffer[10] = (qty >> 8) & 0xFF;     // Quantity high
  buffer[11] = qty & 0xFF;            // Quantity low
}

bool ModbusTcpService::parseModbusResponse(uint8_t* buffer, int length, uint8_t expectedFunc, uint16_t* result, bool* boolResult) {
  if (length < 9) {
    return false;
  }
  
  // Check function code
  uint8_t funcCode = buffer[7];
  if (funcCode != expectedFunc) {
    // Check for error response
    if (funcCode == (expectedFunc | 0x80)) {
      // Modbus error
    }
    return false;
  }
  
  // Parse data based on function code
  if (funcCode == 1 || funcCode == 2) {
    // Coil/discrete input response
    if (length >= 10 && boolResult) {
      uint8_t byteCount = buffer[8];
      if (byteCount > 0) {
        *boolResult = (buffer[9] & 0x01) != 0;
        return true;
      }
    }
  } else if (funcCode == 3 || funcCode == 4) {
    // Register response
    if (length >= 11 && result) {
      uint8_t byteCount = buffer[8];
      if (byteCount >= 2) {
        *result = (buffer[9] << 8) | buffer[10];
        return true;
      }
    }
  }
  
  return false;
}

bool ModbusTcpService::parseMultiModbusResponse(uint8_t* buffer, int length, uint8_t expectedFunc, int count, uint16_t* results) {
  if (length < (9 + count * 2)) {
    return false;
  }
  
  // Check function code
  uint8_t funcCode = buffer[7];
  if (funcCode != expectedFunc) {
    return false;
  }
  
  // Parse multiple registers
  if (funcCode == 3 || funcCode == 4) {
    uint8_t byteCount = buffer[8];
    if (byteCount >= (count * 2)) {
      for (int i = 0; i < count; i++) {
        results[i] = (buffer[9 + i * 2] << 8) | buffer[10 + i * 2];
      }
      return true;
    }
  }
  
  return false;
}

float ModbusTcpService::processMultiRegisterValue(const JsonObject& reg, uint16_t* values, int count) {
  String dataType = reg["data_type"];
  
  if (count == 2) {
    uint32_t combined;
    if (dataType.endsWith("_BE")) {
      combined = ((uint32_t)values[0] << 16) | values[1];
    } else if (dataType.endsWith("_LE")) {
      combined = ((uint32_t)values[1] << 16) | values[0];
    } else if (dataType.endsWith("_BE_BS")) {
      combined = (((uint32_t)values[0] & 0xFF) << 24) | (((uint32_t)values[0] & 0xFF00) << 8) | 
                 (((uint32_t)values[1] & 0xFF) << 8) | ((uint32_t)values[1] >> 8);
    } else if (dataType.endsWith("_LE_BS")) {
      combined = (((uint32_t)values[1] & 0xFF) << 24) | (((uint32_t)values[1] & 0xFF00) << 8) | 
                 (((uint32_t)values[0] & 0xFF) << 8) | ((uint32_t)values[0] >> 8);
    } else {
      combined = ((uint32_t)values[0] << 16) | values[1]; // Default BE
    }
    
    if (dataType.startsWith("INT32")) {
      return (int32_t)combined;
    } else if (dataType.startsWith("UINT32")) {
      return combined;
    } else if (dataType.startsWith("FLOAT32")) {
      return *(float*)&combined;
    }
  } else if (count == 4) {
    // 64-bit data types - return as double but cast to float for compatibility
    uint64_t combined;
    if (dataType.endsWith("_BE")) {
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) | ((uint64_t)values[2] << 16) | values[3];
    } else if (dataType.endsWith("_LE")) {
      combined = ((uint64_t)values[3] << 48) | ((uint64_t)values[2] << 32) | ((uint64_t)values[1] << 16) | values[0];
    } else {
      combined = ((uint64_t)values[0] << 48) | ((uint64_t)values[1] << 32) | ((uint64_t)values[2] << 16) | values[3];
    }
    
    if (dataType.startsWith("INT64")) {
      return (float)(int64_t)combined;
    } else if (dataType.startsWith("UINT64")) {
      return (float)combined;
    } else if (dataType.startsWith("DOUBLE64")) {
      return (float)(*(double*)&combined);
    }
  }
  
  return values[0]; // Fallback
}

float ModbusTcpService::processRegisterValue(const JsonObject& reg, uint16_t rawValue) {
  String dataType = reg["data_type"];
  
  if (dataType == "int16") {
    return (int16_t)rawValue;
  } else if (dataType == "uint16") {
    return rawValue;
  } else if (dataType == "bool") {
    return rawValue != 0 ? 1.0 : 0.0;
  } else if (dataType == "binary") {
    return rawValue;
  }
  
  // Multi-register types - need to read 2 registers
  // For now return single register value, implement multi-register later
  return rawValue;
}

void ModbusTcpService::storeInt32RegisterValue(const String& deviceId, const JsonObject& reg, int32_t value) {
  QueueManager* queueMgr = QueueManager::getInstance();
  
  DynamicJsonDocument dataDoc(256);
  JsonObject dataPoint = dataDoc.to<JsonObject>();
  
  RTCManager* rtc = RTCManager::getInstance();
  if (rtc) {
    DateTime now = rtc->getCurrentTime();
    dataPoint["time"] = now.unixtime();
  } else {
    dataPoint["time"] = millis();
  }
  dataPoint["name"] = reg["register_name"].as<String>();
  dataPoint["address"] = reg["address"];
  dataPoint["datatype"] = reg["data_type"].as<String>();
  dataPoint["value"] = value;
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  String streamId = crudHandler ? crudHandler->getStreamDeviceId() : "";
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    queueMgr->enqueueStream(dataPoint);
  }
}

void ModbusTcpService::storeUint32RegisterValue(const String& deviceId, const JsonObject& reg, uint32_t value) {
  QueueManager* queueMgr = QueueManager::getInstance();
  
  DynamicJsonDocument dataDoc(256);
  JsonObject dataPoint = dataDoc.to<JsonObject>();
  
  RTCManager* rtc = RTCManager::getInstance();
  if (rtc) {
    DateTime now = rtc->getCurrentTime();
    dataPoint["time"] = now.unixtime();
  } else {
    dataPoint["time"] = millis();
  }
  dataPoint["name"] = reg["register_name"].as<String>();
  dataPoint["address"] = reg["address"];
  dataPoint["datatype"] = reg["data_type"].as<String>();
  dataPoint["value"] = value;
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  String streamId = crudHandler ? crudHandler->getStreamDeviceId() : "";
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    queueMgr->enqueueStream(dataPoint);
  }
}

void ModbusTcpService::storeRegisterValue(const String& deviceId, const JsonObject& reg, float value) {
  QueueManager* queueMgr = QueueManager::getInstance();
  
  DynamicJsonDocument dataDoc(256);
  JsonObject dataPoint = dataDoc.to<JsonObject>();
  
  RTCManager* rtc = RTCManager::getInstance();
  if (rtc) {
    DateTime now = rtc->getCurrentTime();
    dataPoint["time"] = now.unixtime();
  } else {
    dataPoint["time"] = millis();
  }
  dataPoint["name"] = reg["register_name"].as<String>();
  dataPoint["address"] = reg["address"];
  dataPoint["datatype"] = reg["data_type"].as<String>();
  dataPoint["value"] = value;
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  String streamId = crudHandler ? crudHandler->getStreamDeviceId() : "";
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    queueMgr->enqueueStream(dataPoint);
  }
}

void ModbusTcpService::getStatus(JsonObject& status) {
  status["running"] = running;
  status["service_type"] = "modbus_tcp";
  status["ethernet_available"] = ethernetManager->isAvailable();
  
  // Count TCP devices
  DynamicJsonDocument devicesDoc(1024);
  JsonArray devices = devicesDoc.to<JsonArray>();
  configManager->listDevices(devices);
  
  int tcpDeviceCount = 0;
  for (JsonVariant deviceVar : devices) {
    String deviceId = deviceVar.as<String>();
    DynamicJsonDocument deviceDoc(512);
    JsonObject deviceObj = deviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj)) {
      String protocol = deviceObj["protocol"] | "";
      if (protocol == "TCP") {
        tcpDeviceCount++;
      }
    }
  }
  
  status["tcp_device_count"] = tcpDeviceCount;
}

ModbusTcpService::~ModbusTcpService() {
  stop();
}