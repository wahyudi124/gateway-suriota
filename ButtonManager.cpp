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
  // Only initialize LED first, delay button init to avoid boot conflicts
  pinMode(PIN_LED_STATUS, OUTPUT);
  
  // Start in running mode (LED blinking)
  startLedBlink();
  
  Serial.println("ButtonManager LED initialized - Button init delayed");
  Serial.printf("LED pin: GPIO%d\n", PIN_LED_STATUS);
  
  // Create task to initialize button after system is fully running
  xTaskCreate(
    delayedButtonInit,
    "BUTTON_INIT_TASK",
    2048,
    this,
    1,
    nullptr
  );
}

void ButtonManager::tick() {
  button.tick();
  
  // Debug: Show button state and raw pin reading
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    int buttonState = digitalRead(PIN_BUTTON);
    Serial.printf("Button - Pin: %d, Config: %s, BLE: %s, Clicks: %d\n", 
                  buttonState,
                  configMode ? "ON" : "OFF", 
                  bleActive ? "ON" : "OFF", 
                  clickCount);
    lastDebug = millis();
  }
  
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
    Serial.printf("Button CLICK detected - Count: %d\n", instance->clickCount);
  }
}

void ButtonManager::handleLongPressStart() {
  if (instance) {
    instance->longPressStart = millis();
    Serial.println("Button LONG PRESS STARTED - Hold for 8 seconds");
  }
}

void ButtonManager::handleLongPressStop() {
  if (instance) {
    unsigned long pressDuration = millis() - instance->longPressStart;
    Serial.printf("Button LONG PRESS STOPPED - Duration: %lu ms\n", pressDuration);
    
    if (pressDuration >= 8000 && !instance->configMode) {
      Serial.println("8+ seconds detected - Entering CONFIG MODE");
      instance->enterConfigMode();
    } else if (pressDuration >= 8000 && instance->configMode) {
      Serial.println("Already in config mode");
    } else {
      Serial.printf("Press too short (%lu ms) - Need 8000+ ms\n", pressDuration);
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

void ButtonManager::delayedButtonInit(void* parameter) {
  ButtonManager* manager = static_cast<ButtonManager*>(parameter);
  
  // Wait for system to be fully running (after all services started)
  vTaskDelay(pdMS_TO_TICKS(5000));
  
  manager->initButton();
  
  // Task completes after initialization
  vTaskDelete(nullptr);
}

void ButtonManager::initButton() {
  // Now safe to initialize GPIO0 - system is fully running
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  // Configure OneButton library
  button.attachClick(handleClick);
  button.attachLongPressStart(handleLongPressStart);
  button.attachLongPressStop(handleLongPressStop);
  button.setPressTicks(8000); // 8 seconds for long press
  button.setClickTicks(400);  // Click detection time
  button.setDebounceTicks(50); // Debounce time
  
  Serial.println("Button GPIO0 initialized after system startup");
  Serial.printf("Button pin: GPIO%d, LED pin: GPIO%d\n", PIN_BUTTON, PIN_LED_STATUS);
  Serial.printf("Long press threshold: 8000ms\n");
  Serial.println("SAFE: GPIO0 initialized after boot to avoid strapping conflicts");
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