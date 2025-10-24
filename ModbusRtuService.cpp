#include "ModbusRtuService.h"
#include "QueueManager.h"
#include "CRUDHandler.h"
#include "RTCManager.h"

extern CRUDHandler* crudHandler;

ModbusRtuService::ModbusRtuService(ConfigManager* config) 
  : configManager(config), running(false), taskHandle(nullptr), 
    serial1(nullptr), serial2(nullptr), modbus1(nullptr), modbus2(nullptr) {}

bool ModbusRtuService::init() {
  Serial.println("Initializing Modbus RTU service with ModbusMaster library...");
  
  if (!configManager) {
    Serial.println("ConfigManager is null");
    return false;
  }
  
  // Initialize Serial1 for Bus 1
  serial1 = new HardwareSerial(1);
  serial1->begin(9600, SERIAL_8N1, RTU_RX1, RTU_TX1);
  
  // Initialize Serial2 for Bus 2
  serial2 = new HardwareSerial(2);
  serial2->begin(9600, SERIAL_8N1, RTU_RX2, RTU_TX2);
  
  // Initialize ModbusMaster instances
  modbus1 = new ModbusMaster();
  modbus1->begin(1, *serial1);
  
  modbus2 = new ModbusMaster();
  modbus2->begin(1, *serial2);
  
  Serial.println("Modbus RTU service initialized successfully");
  return true;
}



void ModbusRtuService::start() {
  Serial.println("Starting Modbus RTU service...");
  
  if (running) {
    return;
  }
  
  running = true;
  BaseType_t result = xTaskCreatePinnedToCore(
    readRtuDevicesTask,
    "MODBUS_RTU_TASK",
    8192,
    this,
    2,
    &taskHandle,
    1
  );
  
  if (result == pdPASS) {
    Serial.println("Modbus RTU service started successfully");
  } else {
    Serial.println("Failed to create Modbus RTU task");
    running = false;
    taskHandle = nullptr;
  }
}

void ModbusRtuService::stop() {
  running = false;
  if (taskHandle) {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  Serial.println("Modbus RTU service stopped");
}

void ModbusRtuService::readRtuDevicesTask(void* parameter) {
  ModbusRtuService* service = static_cast<ModbusRtuService*>(parameter);
  service->readRtuDevicesLoop();
}

void ModbusRtuService::readRtuDevicesLoop() {
  DeviceTimer deviceTimers[10];
  int timerCount = 0;
  
  while (running) {
    DynamicJsonDocument devicesDoc(2048);
    JsonArray devices = devicesDoc.to<JsonArray>();
    configManager->listDevices(devices);
    
    unsigned long currentTime = millis();
    
    for (JsonVariant deviceVar : devices) {
      if (!running) break;
      
      String deviceId = deviceVar.as<String>();
      
      // Debug: Check device ID validity
      if (deviceId.isEmpty() || deviceId == "{}" || deviceId.length() < 3) {
        Serial.printf("[RTU] Invalid device ID: '%s' (length: %d)\n", deviceId.c_str(), deviceId.length());
        continue;
      }
      
      Serial.printf("[RTU] Processing device: '%s'\n", deviceId.c_str());
      
      DynamicJsonDocument deviceDoc(2048);
      JsonObject deviceObj = deviceDoc.to<JsonObject>();
      if (configManager->readDevice(deviceId, deviceObj)) {
        String protocol = deviceObj["protocol"] | "";
        
        if (protocol == "RTU") {
          int refreshRate = deviceObj["refresh_rate_ms"] | 5000;
          
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
              readRtuDeviceData(deviceObj);
              deviceTimers[timerIndex].lastRead = currentTime;
            }
          }
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void ModbusRtuService::readRtuDeviceData(const JsonObject& deviceConfig) {
  String deviceId = deviceConfig["device_id"] | "UNKNOWN";
  int serialPort = deviceConfig["serial_port"] | 1;
  uint8_t slaveId = deviceConfig["slave_id"] | 1;
  JsonArray registers = deviceConfig["registers"];
  
  if (registers.size() == 0) {
    return;
  }
  
  ModbusMaster* modbus = getModbusForBus(serialPort);
  if (!modbus) {
    return;
  }
  
  // Set slave ID for this device
  Serial.printf("[RTU] Setting slave ID to %d for device %s\n", slaveId, deviceId.c_str());
  if (serialPort == 1) {
    modbus1->begin(slaveId, *serial1);
  } else if (serialPort == 2) {
    modbus2->begin(slaveId, *serial2);
  }
  
  for (JsonVariant regVar : registers) {
    if (!running) break;
    
    JsonObject reg = regVar.as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t address = reg["address"] | 0;
    String registerName = reg["register_name"] | "Unknown";
    
    uint8_t result;
    
    if (functionCode == 1) {
      result = modbus->readCoils(address, 1);
      if (result == modbus->ku8MBSuccess) {
        float value = (modbus->getResponseBuffer(0) & 0x01) ? 1.0 : 0.0;
        storeRegisterValue(deviceId, reg, value);
        Serial.printf("%s: %s = %.0f\n", deviceId.c_str(), registerName.c_str(), value);
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    } else if (functionCode == 2) {
      result = modbus->readDiscreteInputs(address, 1);
      if (result == modbus->ku8MBSuccess) {
        float value = (modbus->getResponseBuffer(0) & 0x01) ? 1.0 : 0.0;
        storeRegisterValue(deviceId, reg, value);
        Serial.printf("%s: %s = %.0f\n", deviceId.c_str(), registerName.c_str(), value);
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    } else if (functionCode == 3 || functionCode == 4) {
      String dataType = reg["data_type"] | "int16";
      int registerCount = 1;
      
      // Determine register count based on data type
      if (dataType.startsWith("INT32") || dataType.startsWith("UINT32") || dataType.startsWith("FLOAT32")) {
        registerCount = 2;
      } else if (dataType.startsWith("INT64") || dataType.startsWith("UINT64") || dataType.startsWith("DOUBLE64")) {
        registerCount = 4;
      }
      
      uint16_t values[4];
      if (readMultipleRegisters(modbus, functionCode, address, registerCount, values)) {
        float value = (registerCount == 1) ? 
                     processRegisterValue(reg, values[0]) : 
                     processMultiRegisterValue(reg, values, registerCount);
        storeRegisterValue(deviceId, reg, value);
        Serial.printf("%s: %s = %.2f\n", deviceId.c_str(), registerName.c_str(), value);
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

float ModbusRtuService::processRegisterValue(const JsonObject& reg, uint16_t rawValue) {
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

void ModbusRtuService::storeRegisterValue(const String& deviceId, const JsonObject& reg, float value) {
  QueueManager* queueMgr = QueueManager::getInstance();
  
  // Create data point in required format
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
  
  Serial.printf("Data queued: %s\n", dataPoint["name"].as<String>().c_str());
  
  // Add to message queue
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  // Check if this device is being streamed
  String streamId = "";
  bool crudHandlerAvailable = (crudHandler != nullptr);
  
  if (crudHandler) {
    streamId = crudHandler->getStreamDeviceId();
  }
  
  Serial.printf("RTU: Device %s, CRUDHandler: %s, StreamID '%s', Match: %s\n", 
                deviceId.c_str(), 
                crudHandlerAvailable ? "OK" : "NULL",
                streamId.c_str(), 
                (streamId == deviceId) ? "YES" : "NO");
                
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    Serial.printf("[RTU] Streaming data for device %s to BLE\n", deviceId.c_str());
    queueMgr->enqueueStream(dataPoint);
  } else if (!streamId.isEmpty() && streamId != deviceId) {
    Serial.printf("[RTU] Device %s not streaming (StreamID: %s)\n", deviceId.c_str(), streamId.c_str());
  } else if (streamId.isEmpty()) {
    Serial.printf("[RTU] No streaming active (StreamID empty)\n");
  }
}

bool ModbusRtuService::readMultipleRegisters(ModbusMaster* modbus, uint8_t functionCode, uint16_t address, int count, uint16_t* values) {
  uint8_t result;
  if (functionCode == 3) {
    result = modbus->readHoldingRegisters(address, count);
  } else {
    result = modbus->readInputRegisters(address, count);
  }
  
  if (result == modbus->ku8MBSuccess) {
    for (int i = 0; i < count; i++) {
      values[i] = modbus->getResponseBuffer(i);
    }
    return true;
  }
  return false;
}

float ModbusRtuService::processMultiRegisterValue(const JsonObject& reg, uint16_t* values, int count) {
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

ModbusMaster* ModbusRtuService::getModbusForBus(int serialPort) {
  if (serialPort == 1) {
    return modbus1;
  } else if (serialPort == 2) {
    return modbus2;
  }
  return nullptr;
}

void ModbusRtuService::getStatus(JsonObject& status) {
  status["running"] = running;
  status["service_type"] = "modbus_rtu";
  
  DynamicJsonDocument devicesDoc(1024);
  JsonArray devices = devicesDoc.to<JsonArray>();
  configManager->listDevices(devices);
  
  int rtuDeviceCount = 0;
  for (JsonVariant deviceVar : devices) {
    String deviceId = deviceVar.as<String>();
    DynamicJsonDocument deviceDoc(512);
    JsonObject deviceObj = deviceDoc.to<JsonObject>();
    if (configManager->readDevice(deviceId, deviceObj)) {
      String protocol = deviceObj["protocol"] | "";
      if (protocol == "RTU") {
        rtuDeviceCount++;
      }
    }
  }
  
  status["rtu_device_count"] = rtuDeviceCount;
}

ModbusRtuService::~ModbusRtuService() {
  stop();
  if (serial1) {
    delete serial1;
  }
  if (serial2) {
    delete serial2;
  }
  if (modbus1) {
    delete modbus1;
  }
  if (modbus2) {
    delete modbus2;
  }
}