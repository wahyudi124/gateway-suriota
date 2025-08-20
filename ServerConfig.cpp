#include "ServerConfig.h"
#include <esp_heap_caps.h>
#include <new>

const char* ServerConfig::CONFIG_FILE = "/server_config.json";

ServerConfig::ServerConfig() {
  // Allocate config in PSRAM
  config = (DynamicJsonDocument*)heap_caps_malloc(sizeof(DynamicJsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (config) {
    new(config) DynamicJsonDocument(4096);
  } else {
    config = new DynamicJsonDocument(4096);
  }
  createDefaultConfig();
}

ServerConfig::~ServerConfig() {
  if (config) {
    config->~DynamicJsonDocument();
    heap_caps_free(config);
  }
}

bool ServerConfig::begin() {
  if (!loadConfig()) {
    Serial.println("No server config found, using defaults");
    return saveConfig();
  }
  Serial.println("ServerConfig initialized");
  return true;
}

void ServerConfig::restartDeviceTask(void* parameter) {
  Serial.println("[RESTART] Device will restart in 5 seconds after server config update...");
  vTaskDelay(pdMS_TO_TICKS(5000));
  Serial.println("[RESTART] Restarting device now!");
  ESP.restart();
}

void ServerConfig::scheduleDeviceRestart() {
  Serial.println("[RESTART] Scheduling device restart after server config update");
  xTaskCreate(
    restartDeviceTask,
    "SERVER_RESTART_TASK",
    2048,
    nullptr,
    1,
    nullptr
  );
}

void ServerConfig::createDefaultConfig() {
  config->clear();
  JsonObject root = config->to<JsonObject>();
  
  // Communication config
  JsonObject comm = root.createNestedObject("communication");
  comm["mode"] = "WIFI";
  comm["connection_mode"] = "Automatic";
  comm["ip_address"] = "192.168.1.100";
  comm["mac_address"] = "00:1A:2B:3C:4D:5E";
  
  JsonObject wifi = comm.createNestedObject("wifi");
  wifi["ssid"] = "MyWiFiNetwork";
  wifi["password"] = "MySecretPassword";
  
  // Protocol and data interval
  root["protocol"] = "mqtt";
  
  JsonObject dataInterval = root.createNestedObject("data_interval");
  dataInterval["value"] = 1000;
  dataInterval["unit"] = "ms";
  
  // MQTT config
  JsonObject mqtt = root.createNestedObject("mqtt_config");
  mqtt["enabled"] = true;
  mqtt["broker_address"] = "demo.thingsboard.io";
  mqtt["broker_port"] = 1883;
  mqtt["client_id"] = "esp32_device";
  mqtt["username"] = "device_token";
  mqtt["password"] = "device_password";
  mqtt["topic_publish"] = "v1/devices/me/telemetry";
  mqtt["topic_subscribe"] = "device/control";
  mqtt["keep_alive"] = 60;
  mqtt["clean_session"] = true;
  mqtt["use_tls"] = false;
  
  // HTTP config
  JsonObject http = root.createNestedObject("http_config");
  http["enabled"] = true;
  http["endpoint_url"] = "https://api.example.com/data";
  http["method"] = "POST";
  http["body_format"] = "json";
  http["timeout"] = 5000;
  http["retry"] = 3;
  
  JsonObject headers = http.createNestedObject("headers");
  headers["Authorization"] = "Bearer token";
  headers["Content-Type"] = "application/json";
}

bool ServerConfig::saveConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) return false;
  
  serializeJson(*config, file);
  file.close();
  return true;
}

bool ServerConfig::loadConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) return false;
  
  DeserializationError error = deserializeJson(*config, file);
  file.close();
  
  if (error) {
    Serial.println("Failed to parse server config");
    return false;
  }
  
  return validateConfig(*config);
}

bool ServerConfig::validateConfig(const JsonDocument& cfg) {
  if (!cfg.containsKey("communication") || !cfg.containsKey("protocol")) {
    return false;
  }
  return true;
}

bool ServerConfig::getConfig(JsonObject& result) {
  for (JsonPair kv : config->as<JsonObject>()) {
    result[kv.key()] = kv.value();
  }
  return true;
}

bool ServerConfig::updateConfig(JsonObjectConst newConfig) {
  // Create temporary config for validation
  DynamicJsonDocument tempConfig(4096);
  tempConfig.set(newConfig);
  
  if (!validateConfig(tempConfig)) {
    return false;
  }
  
  // Update main config
  config->set(newConfig);
  if (saveConfig()) {
    Serial.println("Server configuration updated successfully");
    scheduleDeviceRestart();
    return true;
  }
  return false;
}

bool ServerConfig::getCommunicationConfig(JsonObject& result) {
  if (config->containsKey("communication")) {
    JsonObject comm = (*config)["communication"];
    for (JsonPair kv : comm) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

String ServerConfig::getProtocol() {
  return (*config)["protocol"] | "mqtt";
}

bool ServerConfig::getDataIntervalConfig(JsonObject& result) {
  if (config->containsKey("data_interval")) {
    JsonObject interval = (*config)["data_interval"];
    for (JsonPair kv : interval) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getMqttConfig(JsonObject& result) {
  if (config->containsKey("mqtt_config")) {
    JsonObject mqtt = (*config)["mqtt_config"];
    for (JsonPair kv : mqtt) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}

bool ServerConfig::getHttpConfig(JsonObject& result) {
  if (config->containsKey("http_config")) {
    JsonObject http = (*config)["http_config"];
    for (JsonPair kv : http) {
      result[kv.key()] = kv.value();
    }
    return true;
  }
  return false;
}