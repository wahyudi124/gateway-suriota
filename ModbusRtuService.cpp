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
  DeviceTimer deviceTimers[50]; // Support up to 50 devices
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
          
          if (timerIndex == -1 && timerCount < 50) {
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
  
  // Try batch reading for consecutive registers
  bool processedRegisters[registers.size()];
  for (int i = 0; i < registers.size(); i++) {
    processedRegisters[i] = false;
  }
  
  for (int i = 0; i < registers.size(); i++) {
    if (!running || processedRegisters[i]) continue;
    
    JsonObject reg = registers[i].as<JsonObject>();
    uint8_t functionCode = reg["function_code"] | 3;
    uint16_t startAddress = reg["address"] | 0;
    String dataType = reg["data_type"] | "int16";
    
    // Calculate how many registers this data type needs
    int regSize = 1;
    if (dataType.startsWith("INT32") || dataType.startsWith("UINT32") || dataType.startsWith("FLOAT32")) {
      regSize = 2;
    } else if (dataType.startsWith("INT64") || dataType.startsWith("UINT64") || dataType.startsWith("DOUBLE64")) {
      regSize = 4;
    }
    
    // Find consecutive registers that can be read together
    int totalRegisters = regSize;
    int batchCount = 1;
    
    // Look for more registers that can be batched
    for (int j = i + 1; j < registers.size(); j++) {
      if (processedRegisters[j]) continue;
      
      JsonObject nextReg = registers[j].as<JsonObject>();
      uint8_t nextFunctionCode = nextReg["function_code"] | 3;
      uint16_t nextAddress = nextReg["address"] | 0;
      String nextDataType = nextReg["data_type"] | "int16";
      
      int nextRegSize = 1;
      if (nextDataType.startsWith("INT32") || nextDataType.startsWith("UINT32") || nextDataType.startsWith("FLOAT32")) {
        nextRegSize = 2;
      } else if (nextDataType.startsWith("INT64") || nextDataType.startsWith("UINT64") || nextDataType.startsWith("DOUBLE64")) {
        nextRegSize = 4;
      }
      
      // Check if this register is consecutive and same function code
      if (nextFunctionCode == functionCode && nextAddress == (startAddress + totalRegisters)) {
        totalRegisters += nextRegSize;
        batchCount++;
        if (totalRegisters > 50) break; // Increased batch size for large systems
      }
    }
    
    Serial.printf("[BATCH] Reading %d registers starting from %d (batch of %d register configs)\n", 
                  totalRegisters, startAddress, batchCount);
    
    // Read the batch
    uint16_t values[100]; // Max 100 registers per batch for large systems
    if (readMultipleRegisters(modbus, functionCode, startAddress, totalRegisters, values)) {
      // Process each register in the batch
      int valueIndex = 0;
      for (int k = i; k < registers.size() && valueIndex < totalRegisters; k++) {
        if (processedRegisters[k]) continue;
        
        JsonObject batchReg = registers[k].as<JsonObject>();
        uint16_t batchAddress = batchReg["address"] | 0;
        
        // Check if this register is in our current batch
        if (batchAddress >= startAddress && batchAddress < (startAddress + totalRegisters)) {
          processedRegisters[k] = true;
          
          String batchDataType = batchReg["data_type"] | "int16";
          String batchRegisterName = batchReg["register_name"] | "Unknown";
          
          // Calculate offset in the batch
          int offset = batchAddress - startAddress;
          
          // Process based on data type
          if (batchDataType.startsWith("INT32") || batchDataType.startsWith("UINT32") || batchDataType.startsWith("FLOAT32")) {
            if (offset + 1 < totalRegisters) {
              uint32_t combined;
              Serial.printf("%s: Register[%d]=0x%04X (%d), Register[%d]=0x%04X (%d)\n", 
                           batchDataType.c_str(), offset, values[offset], values[offset], 
                           offset+1, values[offset + 1], values[offset + 1]);
              
              if (batchDataType.endsWith("_BE")) {
                // Big Endian: High word first, Low word second
                combined = ((uint32_t)values[offset] << 16) | values[offset + 1];
                Serial.printf("UINT32_BE = %u\n", combined);
              } else if (batchDataType.endsWith("_LE")) {
                // Little Endian: Low word first, High word second  
                combined = ((uint32_t)values[offset + 1] << 16) | values[offset];
                Serial.printf("UINT32_LE = %u\n", combined);
              } else if (batchDataType.endsWith("_LE_BS")) {
                // Little Endian + Byte Swap
                uint16_t word1 = ((values[offset] & 0xFF) << 8) | ((values[offset] & 0xFF00) >> 8);
                uint16_t word2 = ((values[offset + 1] & 0xFF) << 8) | ((values[offset + 1] & 0xFF00) >> 8);
                combined = ((uint32_t)word2 << 16) | word1;
                Serial.printf("LE_BS: word1=0x%04X, word2=0x%04X, combined=0x%08X = %u\n", 
                             word1, word2, combined, combined);
              } else if (batchDataType.endsWith("_BE_BS")) {
                // Big Endian + Byte Swap
                uint16_t word1 = ((values[offset] & 0xFF) << 8) | ((values[offset] & 0xFF00) >> 8);
                uint16_t word2 = ((values[offset + 1] & 0xFF) << 8) | ((values[offset + 1] & 0xFF00) >> 8);
                combined = ((uint32_t)word1 << 16) | word2;
                Serial.printf("BE_BS: word1=0x%04X, word2=0x%04X, combined=0x%08X = %u\n", 
                             word1, word2, combined, combined);
              } else {
                // Default Big Endian
                combined = ((uint32_t)values[offset] << 16) | values[offset + 1];
                Serial.printf("UINT32_BE = %u\n", combined);
              }
              
              if (batchDataType.startsWith("INT32")) {
                int32_t signedValue = (int32_t)combined;
                storeInt32RegisterValue(deviceId, batchReg, signedValue);
                Serial.printf("INT32_%s = %d\n", batchDataType.substring(5).c_str(), signedValue);
              } else if (batchDataType.startsWith("UINT32")) {
                storeUint32RegisterValue(deviceId, batchReg, combined);
                Serial.printf("UINT32_%s = %u\n", batchDataType.substring(6).c_str(), combined);
              } else if (batchDataType.startsWith("FLOAT32")) {
                float floatValue = *(float*)&combined;
                storeRegisterValue(deviceId, batchReg, floatValue);
                Serial.printf("FLOAT32_%s = %.6f\n", batchDataType.substring(7).c_str(), floatValue);
              }
              valueIndex += 2;
            }
          } else {
            // Single register types
            float value = processRegisterValue(batchReg, values[offset]);
            storeRegisterValue(deviceId, batchReg, value);
            String dataType = batchReg["data_type"] | "int16";
            if (dataType == "int16") {
              Serial.printf("int16 = %d\n", (int16_t)value);
            } else if (dataType == "uint16") {
              Serial.printf("uint16 = %u\n", (uint16_t)value);
            } else if (dataType == "bool") {
              Serial.printf("bool = %s\n", value != 0 ? "true" : "false");
            } else {
              Serial.printf("%s = %.2f\n", dataType.c_str(), value);
            }
            valueIndex += 1;
          }
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  // Process any remaining non-batched registers (fallback)
  for (int i = 0; i < registers.size(); i++) {
    if (!running || processedRegisters[i]) continue;
    
    JsonObject reg = registers[i].as<JsonObject>();
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

        
        // Get original 32-bit value for large integers
        String dataType = reg["data_type"] | "int16";
        float value;
        
        if (registerCount == 1) {
          value = processRegisterValue(reg, values[0]);
        } else if (registerCount == 2 && (dataType.startsWith("INT32") || dataType.startsWith("UINT32"))) {
          // For 32-bit integers, get the original value without float conversion
          uint32_t combined;
          if (dataType.endsWith("_BE")) {
            combined = ((uint32_t)values[0] << 16) | values[1];
          } else if (dataType.endsWith("_LE")) {
            combined = ((uint32_t)values[1] << 16) | values[0];
          } else {
            combined = ((uint32_t)values[0] << 16) | values[1]; // Default BE
          }
          
          if (dataType.startsWith("INT32")) {
            int32_t signedValue = (int32_t)combined;

            // Store as int32 without float conversion
            storeInt32RegisterValue(deviceId, reg, signedValue);
            return;
          } else {

            // Store as uint32 without float conversion  
            storeUint32RegisterValue(deviceId, reg, combined);
            return;
          }
        } else {
          value = processMultiRegisterValue(reg, values, registerCount);
          storeRegisterValue(deviceId, reg, value);
          Serial.printf("%s: %s = %.2f\n", deviceId.c_str(), registerName.c_str(), value);
        }
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

void ModbusRtuService::storeInt32RegisterValue(const String& deviceId, const JsonObject& reg, int32_t value) {
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
  dataPoint["value"] = value; // Store as int32 directly
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  Serial.printf("Data queued (INT32): %s = %d\n", dataPoint["name"].as<String>().c_str(), value);
  
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  String streamId = crudHandler ? crudHandler->getStreamDeviceId() : "";
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    queueMgr->enqueueStream(dataPoint);
  }
}

void ModbusRtuService::storeUint32RegisterValue(const String& deviceId, const JsonObject& reg, uint32_t value) {
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
  dataPoint["value"] = value; // Store as uint32 directly
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  Serial.printf("Data queued (UINT32): %s = %u\n", dataPoint["name"].as<String>().c_str(), value);
  
  if (queueMgr) {
    queueMgr->enqueue(dataPoint);
  }
  
  String streamId = crudHandler ? crudHandler->getStreamDeviceId() : "";
  if (!streamId.isEmpty() && streamId == deviceId && queueMgr) {
    queueMgr->enqueueStream(dataPoint);
  }
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
  
  // Debug: Print raw register values
  Serial.printf("[DEBUG] DataType: %s, Count: %d\n", dataType.c_str(), count);
  for (int i = 0; i < count; i++) {
    Serial.printf("[DEBUG] Register[%d]: 0x%04X (%d)\n", i, values[i], values[i]);
  }
  
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
    
    Serial.printf("[DEBUG] Combined: 0x%08X (%u)\n", combined, combined);
    
    if (dataType.startsWith("INT32")) {
      int32_t result = (int32_t)combined;
      Serial.printf("[DEBUG] INT32 result: %d\n", result);
      // Check if value is too large for float precision
      if (result > 16777216 || result < -16777216) {
        Serial.printf("[WARNING] Value %d may lose precision when converted to float\n", result);
      }
      return (float)result;
    } else if (dataType.startsWith("UINT32")) {
      Serial.printf("[DEBUG] UINT32 result: %u\n", combined);
      // Check if value is too large for float precision
      if (combined > 16777216) {
        Serial.printf("[WARNING] Value %u may lose precision when converted to float\n", combined);
      }
      return (float)combined;
    } else if (dataType.startsWith("FLOAT32")) {
      float result = *(float*)&combined;
      Serial.printf("[DEBUG] FLOAT32 result: %.6f\n", result);
      return result;
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