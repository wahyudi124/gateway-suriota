#include "EthernetManager.h"

EthernetManager* EthernetManager::instance = nullptr;

EthernetManager::EthernetManager() : initialized(false), referenceCount(0) {
  generateMacAddress();
}

EthernetManager* EthernetManager::getInstance() {
  if (!instance) {
    instance = new EthernetManager();
  }
  return instance;
}

void EthernetManager::generateMacAddress() {
  // Generate MAC from ESP32 chip ID
  uint64_t chipid = ESP.getEfuseMac();
  mac[0] = 0x02; // Locally administered
  mac[1] = (chipid >> 32) & 0xFF;
  mac[2] = (chipid >> 24) & 0xFF;
  mac[3] = (chipid >> 16) & 0xFF;
  mac[4] = (chipid >> 8) & 0xFF;
  mac[5] = chipid & 0xFF;
}

bool EthernetManager::init() {
  if (initialized) {
    referenceCount++;
    Serial.printf("Ethernet already initialized (refs: %d)\n", referenceCount);
    return true;
  }
  
  // Configure SPI pins for W5500
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  
  // Initialize Ethernet with DHCP
  Ethernet.init(CS_PIN);
  
  Serial.println("Starting Ethernet with DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    
    // Try with static IP as fallback
    IPAddress ip(192, 168, 1, 177);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    Ethernet.begin(mac, ip, gateway, subnet);
    Serial.printf("Ethernet configured with static IP: %s\n", ip.toString().c_str());
  } else {
    Serial.printf("Ethernet configured with DHCP IP: %s\n", Ethernet.localIP().toString().c_str());
  }
  
  // Check for Ethernet hardware
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found");
    return false;
  }
  
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected");
    return false;
  }
  
  initialized = true;
  referenceCount = 1;
  Serial.println("Ethernet initialized successfully");
  return true;
}

void EthernetManager::addReference() {
  if (initialized) {
    referenceCount++;
    Serial.printf("Ethernet reference added (refs: %d)\n", referenceCount);
  }
}

void EthernetManager::removeReference() {
  if (referenceCount > 0) {
    referenceCount--;
    Serial.printf("Ethernet reference removed (refs: %d)\n", referenceCount);
    
    if (referenceCount == 0) {
      cleanup();
    }
  }
}

void EthernetManager::cleanup() {
  referenceCount = 0;
  initialized = false;
  Serial.println("Ethernet resources cleaned up");
}

bool EthernetManager::isAvailable() {
  if (!initialized) return false;
  
  // Check link status
  return Ethernet.linkStatus() == LinkON;
}

IPAddress EthernetManager::getLocalIP() {
  if (initialized) {
    return Ethernet.localIP();
  }
  return IPAddress(0, 0, 0, 0);
}

void EthernetManager::getStatus(JsonObject& status) {
  status["initialized"] = initialized;
  status["available"] = isAvailable();
  status["reference_count"] = referenceCount;
  
  if (initialized) {
    status["ip_address"] = getLocalIP().toString();
    status["link_status"] = (Ethernet.linkStatus() == LinkON) ? "connected" : "disconnected";
    status["hardware_status"] = (Ethernet.hardwareStatus() == EthernetW5500) ? "W5500" : "unknown";
  }
}

EthernetManager::~EthernetManager() {
  cleanup();
}