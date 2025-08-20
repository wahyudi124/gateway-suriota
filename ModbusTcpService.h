#ifndef MODBUS_TCP_SERVICE_H
#define MODBUS_TCP_SERVICE_H

#include <ArduinoJson.h>
#include <Ethernet.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ConfigManager.h"
#include "EthernetManager.h"

class ModbusTcpService {
private:
  ConfigManager* configManager;
  EthernetManager* ethernetManager;
  bool running;
  TaskHandle_t taskHandle;
  
  // Device timers for refresh rate control
  struct DeviceTimer {
    String deviceId;
    unsigned long lastRead;
  };
  
  // Modbus TCP protocol implementation
  struct ModbusFrame {
    uint16_t transactionId;
    uint16_t protocolId;
    uint16_t length;
    uint8_t unitId;
    uint8_t functionCode;
    uint16_t startAddress;
    uint16_t quantity;
  };
  
  static uint16_t transactionCounter;
  
  static void readTcpDevicesTask(void* parameter);
  void readTcpDevicesLoop();
  void readTcpDeviceData(const JsonObject& deviceConfig);
  bool readModbusRegister(const String& ip, int port, uint8_t slaveId, uint8_t functionCode, uint16_t address, uint16_t* result);
  bool readModbusCoil(const String& ip, int port, uint8_t slaveId, uint16_t address, bool* result);
  void buildModbusRequest(uint8_t* buffer, uint16_t transId, uint8_t unitId, uint8_t funcCode, uint16_t addr, uint16_t qty);
  bool parseModbusResponse(uint8_t* buffer, int length, uint8_t expectedFunc, uint16_t* result, bool* boolResult = nullptr);
  float processRegisterValue(const JsonObject& reg, uint16_t rawValue);
  void storeRegisterValue(const String& deviceId, const JsonObject& reg, float value);

public:
  ModbusTcpService(ConfigManager* config, EthernetManager* ethernet);
  
  bool init();
  void start();
  void stop();
  void getStatus(JsonObject& status);
  
  ~ModbusTcpService();
};

#endif