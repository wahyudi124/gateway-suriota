#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

#define PIN_BUTTON 0
#define PIN_LED_STATUS 7

class ButtonManager {
private:
  bool configMode;
  bool bleActive;
  unsigned long longPressStart;
  int clickCount;
  unsigned long lastClickTime;
  TaskHandle_t ledTaskHandle;
  bool buttonInitialized;
  bool _longclick;
  bool _singleclick;
  
  // Native button handling
  int lastButtonState;
  int buttonState;
  unsigned long lastDebounceTime;
  
  static void ledBlinkTask(void* parameter);
  static void delayedButtonInit(void* parameter);
  void startLedBlink();
  void stopLedBlink();
  void setLedSolid(bool on);
  void initButton();
  

  
  static ButtonManager* instance;
  
public:
  ButtonManager();
  
  void begin();
  void tick();
  
  bool isConfigMode() const { return configMode; }
  bool isBleActive() const { return bleActive; }
  
  void enterConfigMode();
  void exitConfigMode();
  
  static ButtonManager* getInstance();
};

#endif