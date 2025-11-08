#include "ButtonManager.h"
#include "BLEManager.h"

ButtonManager* ButtonManager::instance = nullptr;
extern BLEManager* bleManager;

ButtonManager::ButtonManager() : button(PIN_BUTTON, true), configMode(false), bleActive(false), 
                                longPressStart(0), clickCount(0), lastClickTime(0), ledTaskHandle(nullptr) {}

ButtonManager* ButtonManager::getInstance() {
  if (!instance) {
    instance = new ButtonManager();
  }
  return instance;
}

void ButtonManager::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_STATUS, OUTPUT);
  
  button.attachClick(handleClick);
  button.attachLongPressStart(handleLongPressStart);
  button.attachLongPressStop(handleLongPressStop);
  button.setPressTicks(8000); // 8 seconds for long press
  
  // Start in running mode (LED blinking)
  startLedBlink();
  
  Serial.println("ButtonManager initialized");
}

void ButtonManager::tick() {
  button.tick();
  
  // Handle triple click detection
  if (clickCount > 0 && millis() - lastClickTime > 1000) {
    if (clickCount == 3 && configMode) {
      exitConfigMode();
    }
    clickCount = 0;
  }
}

void ButtonManager::handleClick() {
  if (instance) {
    instance->clickCount++;
    instance->lastClickTime = millis();
    Serial.printf("Click count: %d\n", instance->clickCount);
  }
}

void ButtonManager::handleLongPressStart() {
  if (instance) {
    instance->longPressStart = millis();
    Serial.println("Long press started");
  }
}

void ButtonManager::handleLongPressStop() {
  if (instance) {
    unsigned long pressDuration = millis() - instance->longPressStart;
    Serial.printf("Long press duration: %lu ms\n", pressDuration);
    
    if (pressDuration >= 8000 && !instance->configMode) {
      instance->enterConfigMode();
    }
  }
}

void ButtonManager::enterConfigMode() {
  if (configMode) return;
  
  configMode = true;
  bleActive = true;
  
  // Stop blinking LED and turn on solid
  stopLedBlink();
  setLedSolid(true);
  
  // Start BLE advertising
  if (bleManager) {
    bleManager->startAdvertising();
    Serial.println("BLE Advertising started for config mode");
  }
  
  Serial.println("Entered CONFIG MODE - BLE Active, LED Solid ON");
}

void ButtonManager::exitConfigMode() {
  if (!configMode) return;
  
  configMode = false;
  bleActive = false;
  
  // Turn off solid LED and start blinking
  setLedSolid(false);
  startLedBlink();
  
  // Stop BLE advertising
  if (bleManager) {
    bleManager->stopAdvertising();
    Serial.println("BLE Advertising stopped for running mode");
  }
  
  clickCount = 0; // Reset click counter
  
  Serial.println("Exited CONFIG MODE - BLE Inactive, LED Blinking");
}

void ButtonManager::startLedBlink() {
  if (ledTaskHandle) return; // Already running
  
  xTaskCreate(
    ledBlinkTask,
    "LED_BLINK_TASK",
    2048,
    this,
    1,
    &ledTaskHandle
  );
}

void ButtonManager::stopLedBlink() {
  if (ledTaskHandle) {
    vTaskDelete(ledTaskHandle);
    ledTaskHandle = nullptr;
  }
}

void ButtonManager::setLedSolid(bool on) {
  digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
}

void ButtonManager::ledBlinkTask(void* parameter) {
  ButtonManager* manager = static_cast<ButtonManager*>(parameter);
  
  while (true) {
    if (!manager->configMode) {
      // Slow blink in running mode
      digitalWrite(PIN_LED_STATUS, HIGH);
      vTaskDelay(pdMS_TO_TICKS(1000));
      digitalWrite(PIN_LED_STATUS, LOW);
      vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
      // Task should be deleted when entering config mode
      break;
    }
  }
  
  manager->ledTaskHandle = nullptr;
  vTaskDelete(nullptr);
}