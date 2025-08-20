#ifndef MODBUS_RTU_SERVICE_H
#define MODBUS_RTU_SERVICE_H

#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ModbusMaster.h>
#include "ConfigManager.h"

class ModbusRtuService {
private:
  ConfigManager* configManager;
  bool running;
  TaskHandle_t taskHandle;
  
  // Hardware configuration for dual RTU buses
  static const int RTU_RX1 = 15;   // GPIO15 RXD1_RS485
  static const int RTU_TX1 = 16;   // GPIO16 TXD1_RS485
  
  static const int RTU_RX2 = 17;   // GPIO17 RXD2_RS485
  static const int RTU_TX2 = 18;   // GPIO18 TXD2_RS485
  
  HardwareSerial* serial1;
  HardwareSerial* serial2;
  ModbusMaster* modbus1;
  ModbusMaster* modbus2;
  
  // Device timers for refresh rate control
  struct DeviceTimer {
    String deviceId;
    unsigned long lastRead;
  };
  

  
  static void readRtuDevicesTask(void* parameter);
  void readRtuDevicesLoop();
  void readRtuDeviceData(const JsonObject& deviceConfig);
  float processRegisterValue(const JsonObject& reg, uint16_t rawValue);
  void storeRegisterValue(const String& deviceId, const JsonObject& reg, float value);
  ModbusMaster* getModbusForBus(int serialPort);

public:
  ModbusRtuService(ConfigManager* config);
  
  bool init();
  void start();
  void stop();
  void getStatus(JsonObject& status);
  
  ~ModbusRtuService();
};

#endif