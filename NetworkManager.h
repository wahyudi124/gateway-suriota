#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <ArduinoJson.h>
#include "WiFiManager.h"
#include "EthernetManager.h"

class NetworkMgr {
private:
  static NetworkMgr* instance;
  WiFiManager* wifiManager;
  EthernetManager* ethernetManager;
  String currentMode;
  bool networkAvailable;
  
  NetworkMgr();
  bool initWiFi(const JsonObject& wifiConfig);
  bool initEthernet();

public:
  static NetworkMgr* getInstance();
  
  bool init(const JsonObject& serverConfig);
  bool isAvailable();
  IPAddress getLocalIP();
  String getCurrentMode();
  void cleanup();
  void getStatus(JsonObject& status);
  
  ~NetworkMgr();
};

#endif