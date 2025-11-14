/*
 * BLE CRUD Manager for ESP32
 * Production-ready BLE service for Modbus device configuration
 * 
 * Hardware: ESP32 with BLE support
 * Features: CRUD operations, fragmentation, FreeRTOS tasks
 */

#include "BLEManager.h"
#include "CRUDHandler.h"
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "LoggingConfig.h"
#include "NetworkManager.h"
#include "RTCManager.h"
#include "ModbusTcpService.h"
#include "ModbusRtuService.h"
#include "QueueManager.h"
#include "MqttManager.h"
#include "HttpManager.h"
#include "SDLogger.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>

// Add missing include
#include <new>

// Button and LED pins
#define CONFIG_BUTTON_PIN 0
#define STATUS_LED_PIN 8

// Button state variables
volatile unsigned long buttonPressTime = 0;
volatile unsigned long buttonReleaseTime = 0;
volatile int clickCount = 0;
volatile unsigned long lastClickTime = 0;
volatile bool buttonPressed = false;
bool bleConfigMode = false;

// Global objects - initialized to nullptr for safety
BLEManager* bleManager = nullptr;
CRUDHandler* crudHandler = nullptr;
ConfigManager* configManager = nullptr;
ServerConfig* serverConfig = nullptr;
LoggingConfig* loggingConfig = nullptr;
NetworkMgr* networkManager = nullptr;
RTCManager* rtcManager = nullptr;
ModbusTcpService* modbusTcpService = nullptr;
ModbusRtuService* modbusRtuService = nullptr;
QueueManager* queueManager = nullptr;
MqttManager* mqttManager = nullptr;
HttpManager* httpManager = nullptr;
SDLogger* sdLogger = nullptr;

// Cleanup function for failed initialization
void cleanup() {
  if (configManager) { configManager->~ConfigManager(); heap_caps_free(configManager); }
  if (serverConfig) delete serverConfig;
  if (loggingConfig) delete loggingConfig;
  if (modbusTcpService) delete modbusTcpService;
  if (modbusRtuService) delete modbusRtuService;
  if (crudHandler) { crudHandler->~CRUDHandler(); heap_caps_free(crudHandler); }
  if (bleManager) { bleManager->~BLEManager(); heap_caps_free(bleManager); }
  if (sdLogger) delete sdLogger;
}

void IRAM_ATTR buttonISR() {
  unsigned long now = millis();
  bool currentState = digitalRead(CONFIG_BUTTON_PIN);
  
  if (currentState == LOW && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = now;
  } else if (currentState == HIGH && buttonPressed) {
    buttonPressed = false;
    buttonReleaseTime = now;
    
    unsigned long pressDuration = buttonReleaseTime - buttonPressTime;
    
    if (pressDuration < 8000) {
      if (now - lastClickTime < 1000) {
        clickCount++;
      } else {
        clickCount = 1;
      }
      lastClickTime = now;
    }
  }
}

void checkButtonAction() {
  unsigned long now = millis();
  
  // Check for long press (>8 seconds)
  if (buttonPressed && (now - buttonPressTime > 8000) && !bleConfigMode) {
    bleConfigMode = true;
    if (bleManager) {
      bleManager->begin();
      digitalWrite(STATUS_LED_PIN, HIGH);
      Serial.println("BLE Config Mode: ACTIVE");
    }
    buttonPressed = false;
  }
  
  // Check for triple click
  if (clickCount >= 3 && (now - lastClickTime > 500) && bleConfigMode) {
    bleConfigMode = false;
    if (bleManager) {
      bleManager->stop();
      Serial.println("BLE Config Mode: INACTIVE");
    }
    clickCount = 0;
  }
  
  // Reset click count after timeout
  if (clickCount > 0 && (now - lastClickTime > 1000)) {
    clickCount = 0;
  }
}

void ledStatusTask(void* parameter) {
  while (true) {
    if (bleConfigMode) {
      digitalWrite(STATUS_LED_PIN, HIGH);
    } else {
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Initialize button and LED
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(CONFIG_BUTTON_PIN), buttonISR, CHANGE);
  
  // Start LED status task
  xTaskCreate(ledStatusTask, "LED_Status", 2048, NULL, 1, NULL);
  
  // Check PSRAM availability
  if (esp_psram_is_initialized()) {
    Serial.printf("PSRAM available: %d bytes\n", ESP.getPsramSize());
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("PSRAM not available - using internal RAM");
  }
  
  Serial.println("Starting BLE CRUD Manager...");
  
  // Initialize configuration manager in PSRAM
  configManager = (ConfigManager*)heap_caps_malloc(sizeof(ConfigManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (configManager) {
    new(configManager) ConfigManager();
  } else {
    configManager = new ConfigManager(); // Fallback to internal RAM
    if (!configManager) {
      Serial.println("Failed to allocate ConfigManager");
      return;
    }
  }
  
  if (!configManager->begin()) {
    Serial.println("Failed to initialize ConfigManager");
    cleanup();
    return;
  }
  
  // Debug: Show devices file content
  configManager->debugDevicesFile();

    // Force clear all cache and reload
  configManager->clearAllConfigurations();
  
  // Create test device if no valid devices exist

  
  // Clear all existing configurations for fresh start
  //configManager->clearAllConfigurations(); // Commented out to preserve existing devices

  
  // Fix corrupt device IDs by recreating clean file
  configManager->fixCorruptDeviceIds();
  
  Serial.println("[MAIN] Configuration initialization completed.");
  
  // Initialize queue manager
  queueManager = QueueManager::getInstance();
  if (!queueManager || !queueManager->init()) {
    Serial.println("Failed to initialize QueueManager");
    cleanup();
    return;
  }
  
  // Initialize server config
  serverConfig = new ServerConfig();
  if (!serverConfig || !serverConfig->begin()) {
    Serial.println("Failed to initialize ServerConfig");
    cleanup();
    return;
  }
  
  // Initialize logging config
  loggingConfig = new LoggingConfig();
  if (!loggingConfig || !loggingConfig->begin()) {
    Serial.println("Failed to initialize LoggingConfig");
    cleanup();
    return;
  }
  
  // Initialize network manager
  networkManager = NetworkMgr::getInstance();
  if (!networkManager) {
    Serial.println("Failed to get NetworkManager instance");
    cleanup();
    return;
  }
  DynamicJsonDocument serverConfigDoc(2048);
  JsonObject serverConfigObj = serverConfigDoc.to<JsonObject>();
  if (serverConfig->getConfig(serverConfigObj)) {
    if (!networkManager->init(serverConfigObj)) {
      Serial.println("Failed to initialize NetworkManager");
    }
  }
  
  // Initialize RTC manager
  rtcManager = RTCManager::getInstance();
  if (!rtcManager || !rtcManager->init()) {
    Serial.println("Failed to initialize RTCManager");
  } else {
    rtcManager->startSync();
  }
  
  // Initialize SD Logger
  sdLogger = new SDLogger(loggingConfig, rtcManager);
  if (!sdLogger || !sdLogger->begin()) {
    Serial.println("Failed to initialize SDLogger");
  } else {
    Serial.println("SDLogger initialized successfully");
  }
  
  // Initialize Modbus TCP service
  EthernetManager* ethernetMgr = EthernetManager::getInstance();
  if (ethernetMgr) {
    modbusTcpService = new ModbusTcpService(configManager, ethernetMgr);
    if (modbusTcpService && modbusTcpService->init()) {
      modbusTcpService->start();
      Serial.println("Modbus TCP service started");
    } else {
      Serial.println("Failed to initialize Modbus TCP service");
    }
  }
  
  // Initialize CRUD handler in PSRAM FIRST (before Modbus services)
  crudHandler = (CRUDHandler*)heap_caps_malloc(sizeof(CRUDHandler), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (crudHandler) {
    new(crudHandler) CRUDHandler(configManager, serverConfig, loggingConfig);
  } else {
    crudHandler = new CRUDHandler(configManager, serverConfig, loggingConfig); // Fallback to internal RAM
    if (!crudHandler) {
      Serial.println("Failed to allocate CRUDHandler");
      cleanup();
      return;
    }
  }
  Serial.println("CRUDHandler initialized");
  
  // Initialize Modbus RTU service (after CRUDHandler)
  modbusRtuService = new ModbusRtuService(configManager);
  if (modbusRtuService && modbusRtuService->init()) {
    modbusRtuService->start();
    Serial.println("Modbus RTU service started");
  } else {
    Serial.println("Failed to initialize Modbus RTU service");
  }
  
  // Initialize protocol managers based on server configuration
  String protocol = serverConfig->getProtocol();
  Serial.printf("Selected protocol: %s\n", protocol.c_str());
  
  // Initialize MQTT Manager
  mqttManager = MqttManager::getInstance(configManager, serverConfig, networkManager);
  if (mqttManager && mqttManager->init()) {
    if (protocol == "mqtt") {
      mqttManager->start();
      Serial.println("MQTT Manager started (active protocol)");
    } else {
      Serial.println("MQTT Manager initialized but not started (inactive protocol)");
    }
  } else {
    Serial.println("Failed to initialize MQTT Manager");
  }
  
  // Initialize HTTP Manager
  httpManager = HttpManager::getInstance(configManager, serverConfig, networkManager);
  if (httpManager && httpManager->init()) {
    if (protocol == "http") {
      httpManager->start();
      Serial.println("HTTP Manager started (active protocol)");
    } else {
      Serial.println("HTTP Manager initialized but not started (inactive protocol)");
    }
  } else {
    Serial.println("Failed to initialize HTTP Manager");
  }
  
  // CRUDHandler already initialized above
  
  // Initialize BLE manager in PSRAM (but don't start yet)
  bleManager = (BLEManager*)heap_caps_malloc(sizeof(BLEManager), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (bleManager) {
    new(bleManager) BLEManager("SURIOTA GW", crudHandler);
  } else {
    bleManager = new BLEManager("SURIOTA GW", crudHandler); // Fallback to internal RAM
    if (!bleManager) {
      Serial.println("Failed to allocate BLE Manager");
      cleanup();
      return;
    }
  }
  // BLE will be started by button press
  Serial.println("BLE Manager ready (press config button >8s to activate)");
  
  Serial.println("BLE CRUD Manager started successfully");
}

void loop() {
  checkButtonAction();
  
  // FreeRTOS-friendly delay - yields to other tasks
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Optional: Add watchdog feed or system monitoring here
  // esp_task_wdt_reset();
}