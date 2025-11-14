#include "ButtonManager.h"
#include "BLEManager.h"

ButtonManager* ButtonManager::instance = nullptr;
extern BLEManager* bleManager;

ButtonManager::ButtonManager() : configMode(false), bleActive(false), 
                                longPressStart(0), clickCount(0), lastClickTime(0), ledTaskHandle(nullptr),
                                buttonInitialized(false), _longclick(false), _singleclick(false),
                                lastButtonState(HIGH), buttonState(HIGH), lastDebounceTime(0) {}

ButtonManager* ButtonManager::getInstance() {
  if (!instance) {
    instance = new ButtonManager();
  }
  return instance;
}

void ButtonManager::begin() {
  // Initialize LED only
  pinMode(PIN_LED_STATUS, OUTPUT);
  
  // Start in running mode (LED blinking)
  startLedBlink();
  
  // Delay button initialization for 20 seconds
  xTaskCreate(
    delayedButtonInit,
    "BUTTON_INIT_TASK",
    2048,
    this,
    1,
    nullptr
  );
  
  Serial.println("ButtonManager LED initialized - Button init delayed 20s");
}

void ButtonManager::tick() {
  if (!buttonInitialized) return;
  
  int reading = digitalRead(PIN_BUTTON);
  
  // Debug: Print button state every 5 seconds
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 5000) {
    Serial.printf("Button state: %d (HIGH=1, LOW=0)\n", reading);
    lastDebugTime = millis();
  }
  
  // Debounce
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) { // Button pressed (INPUT_PULLUP goes LOW when pressed)
        longPressStart = millis();
        Serial.println("*** BUTTON PRESSED ***");
      } else { // Button released
        if (longPressStart > 0) {
          unsigned long pressDuration = millis() - longPressStart;
          Serial.printf("*** BUTTON RELEASED after %lums ***\n", pressDuration);
          
          if (pressDuration >= 8000) {
            if (!configMode) {
              Serial.println("LONG PRESS - ENTERING CONFIG MODE");
              enterConfigMode();
            }
          } else if (pressDuration >= 100) {
            if (configMode) {
              Serial.println("SHORT PRESS - EXITING CONFIG MODE");
              exitConfigMode();
            }
          }
          longPressStart = 0;
        }
      }
    }
  }
  
  lastButtonState = reading;
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
  
  // Wait 20 seconds for system to stabilize
  vTaskDelay(pdMS_TO_TICKS(20000));
  
  manager->initButton();
  
  vTaskDelete(nullptr);
}

void ButtonManager::initButton() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  
  buttonState = HIGH;
  lastButtonState = HIGH;
  buttonInitialized = true;
  
  Serial.println("Button GPIO0 initialized after 20s delay - ready for input");
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