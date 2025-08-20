#ifndef ETHERNET_MANAGER_H
#define ETHERNET_MANAGER_H

#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

class EthernetManager {
private:
  static EthernetManager* instance;
  bool initialized;
  int referenceCount;
  byte mac[6];
  
  // W5500 pins
  static const int CS_PIN = 48;
  static const int INT_PIN = 9;
  static const int MOSI_PIN = 14;
  static const int MISO_PIN = 21;
  static const int SCK_PIN = 47;
  
  EthernetManager();
  void generateMacAddress();

public:
  static EthernetManager* getInstance();
  
  bool init();
  void addReference();
  void removeReference();
  void cleanup();
  
  bool isAvailable();
  IPAddress getLocalIP();
  void getStatus(JsonObject& status);
  
  ~EthernetManager();
};

#endif