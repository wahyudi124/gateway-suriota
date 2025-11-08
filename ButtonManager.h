#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <OneButton.h>

#define PIN_BUTTON 0
#define PIN_LED_STATUS 7

class ButtonManager {
private:
  OneButton button;
  bool configMode;
  bool bleActive;
  unsigned long longPressStart;
  int clickCount;
  unsigned long lastClickTime;
  TaskHandle_t ledTaskHandle;
  
  static void ledBlinkTask(void* parameter);
  void startLedBlink();
  void stopLedBlink();
  void setLedSolid(bool on);
  
  static void handleClick();
  static void handleLongPressStart();
  static void handleLongPressStop();
  
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