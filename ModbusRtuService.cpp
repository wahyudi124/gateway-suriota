#include "ModbusRtuService.h"
#include "QueueManager.h"

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
    } else if (functionCode == 3) {
      result = modbus->readHoldingRegisters(address, 1);
      if (result == modbus->ku8MBSuccess) {
        uint16_t rawValue = modbus->getResponseBuffer(0);
        float value = processRegisterValue(reg, rawValue);
        storeRegisterValue(deviceId, reg, value);
        Serial.printf("%s: %s = %.2f\n", deviceId.c_str(), registerName.c_str(), value);
      } else {
        Serial.printf("%s: %s = ERROR\n", deviceId.c_str(), registerName.c_str());
      }
    } else if (functionCode == 4) {
      result = modbus->readInputRegisters(address, 1);
      if (result == modbus->ku8MBSuccess) {
        uint16_t rawValue = modbus->getResponseBuffer(0);
        float value = processRegisterValue(reg, rawValue);
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
  } else if (dataType == "int32") {
    return rawValue;
  } else if (dataType == "float32") {
    return rawValue / 100.0;
  } else if (dataType == "bool") {
    return rawValue != 0 ? 1.0 : 0.0;
  }
  
  return rawValue;
}

void ModbusRtuService::storeRegisterValue(const String& deviceId, const JsonObject& reg, float value) {
  QueueManager* queueMgr = QueueManager::getInstance();
  
  // Create data point in required format
  DynamicJsonDocument dataDoc(256);
  JsonObject dataPoint = dataDoc.to<JsonObject>();
  
  dataPoint["time"] = millis();
  dataPoint["name"] = reg["register_name"].as<String>();
  dataPoint["address"] = reg["address"];
  dataPoint["datatype"] = reg["data_type"].as<String>();
  dataPoint["value"] = value;
  dataPoint["device_id"] = deviceId;
  dataPoint["register_id"] = reg["register_id"].as<String>();
  
  // Add to message queue
  queueMgr->enqueue(dataPoint);
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