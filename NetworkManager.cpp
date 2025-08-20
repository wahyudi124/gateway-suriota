#include "NetworkManager.h"

NetworkMgr* NetworkMgr::instance = nullptr;

NetworkMgr::NetworkMgr() : wifiManager(nullptr), ethernetManager(nullptr), networkAvailable(false) {}

NetworkMgr* NetworkMgr::getInstance() {
  if (!instance) {
    instance = new NetworkMgr();
  }
  return instance;
}

bool NetworkMgr::init(const JsonObject& serverConfig) {
  if (!serverConfig.containsKey("communication")) {
    Serial.println("No communication config found");
    return false;
  }
  
  JsonObject commConfig = serverConfig["communication"];
  String mode = commConfig["mode"] | "WIFI";
  
  if (mode == "WIFI") {
    if (commConfig.containsKey("wifi")) {
      return initWiFi(commConfig["wifi"]);
    } else {
      Serial.println("WiFi config not found");
      return false;
    }
  } else if (mode == "ETH") {
    return initEthernet();
  } else {
    Serial.printf("Unknown network mode: %s\n", mode.c_str());
    return false;
  }
}

bool NetworkMgr::initWiFi(const JsonObject& wifiConfig) {
  String ssid = wifiConfig["ssid"] | "";
  String password = wifiConfig["password"] | "";
  
  if (ssid.isEmpty()) {
    Serial.println("WiFi SSID not provided");
    return false;
  }
  
  wifiManager = WiFiManager::getInstance();
  if (wifiManager->init(ssid, password)) {
    currentMode = "WIFI";
    networkAvailable = true;
    Serial.printf("Network initialized: WiFi (%s)\n", ssid.c_str());
    return true;
  }
  
  return false;
}

bool NetworkMgr::initEthernet() {
  ethernetManager = EthernetManager::getInstance();
  if (ethernetManager->init()) {
    currentMode = "ETH";
    networkAvailable = true;
    Serial.println("Network initialized: Ethernet");
    return true;
  }
  
  return false;
}

bool NetworkMgr::isAvailable() {
  if (currentMode == "WIFI" && wifiManager) {
    return wifiManager->isAvailable();
  } else if (currentMode == "ETH" && ethernetManager) {
    return ethernetManager->isAvailable();
  }
  return false;
}

IPAddress NetworkMgr::getLocalIP() {
  if (currentMode == "WIFI" && wifiManager) {
    return wifiManager->getLocalIP();
  } else if (currentMode == "ETH" && ethernetManager) {
    return ethernetManager->getLocalIP();
  }
  return IPAddress(0, 0, 0, 0);
}

String NetworkMgr::getCurrentMode() {
  return currentMode;
}

void NetworkMgr::cleanup() {
  if (wifiManager) {
    wifiManager->cleanup();
  }
  if (ethernetManager) {
    ethernetManager->cleanup();
  }
  networkAvailable = false;
  currentMode = "";
}

void NetworkMgr::getStatus(JsonObject& status) {
  status["mode"] = currentMode;
  status["available"] = isAvailable();
  status["ip_address"] = getLocalIP().toString();
  
  if (currentMode == "WIFI" && wifiManager) {
    JsonObject wifiStatus = status.createNestedObject("wifi_status");
    wifiManager->getStatus(wifiStatus);
  } else if (currentMode == "ETH" && ethernetManager) {
    JsonObject ethStatus = status.createNestedObject("ethernet_status");
    ethernetManager->getStatus(ethStatus);
  }
}

NetworkMgr::~NetworkMgr() {
  cleanup();
}