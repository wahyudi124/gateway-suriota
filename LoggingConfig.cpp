#include "LoggingConfig.h"

const char* LoggingConfig::CONFIG_FILE = "/logging_config.json";

LoggingConfig::LoggingConfig() : config(256) {
  createDefaultConfig();
}

bool LoggingConfig::begin() {
  if (!loadConfig()) {
    Serial.println("No logging config found, using defaults");
    return saveConfig();
  }
  Serial.println("LoggingConfig initialized");
  return true;
}

void LoggingConfig::createDefaultConfig() {
  config.clear();
  config["logging_ret"] = "1m";
  config["logging_interval"] = "10m";
}

bool LoggingConfig::saveConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) return false;
  
  serializeJson(config, file);
  file.close();
  return true;
}

bool LoggingConfig::loadConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) return false;
  
  DeserializationError error = deserializeJson(config, file);
  file.close();
  
  if (error) {
    Serial.println("Failed to parse logging config");
    return false;
  }
  
  return validateConfig(config);
}

bool LoggingConfig::validateConfig(const JsonDocument& cfg) {
  if (!cfg.containsKey("logging_ret") || !cfg.containsKey("logging_interval")) {
    return false;
  }
  
  // Validate retention values
  String ret = cfg["logging_ret"];
  if (ret != "1w" && ret != "1m" && ret != "3m") {
    return false;
  }
  
  // Validate interval values
  String interval = cfg["logging_interval"];
  if (interval != "5m" && interval != "10m" && interval != "30m") {
    return false;
  }
  
  return true;
}

bool LoggingConfig::getConfig(JsonObject& result) {
  for (JsonPair kv : config.as<JsonObject>()) {
    result[kv.key()] = kv.value();
  }
  return true;
}

bool LoggingConfig::updateConfig(JsonObjectConst newConfig) {
  // Create temporary config for validation
  DynamicJsonDocument tempConfig(256);
  tempConfig.set(newConfig);
  
  if (!validateConfig(tempConfig)) {
    return false;
  }
  
  // Update main config
  config.set(newConfig);
  return saveConfig();
}

String LoggingConfig::getLoggingRetention() {
  return config["logging_ret"] | "1w";
}

String LoggingConfig::getLoggingInterval() {
  return config["logging_interval"] | "5m";
}

unsigned long LoggingConfig::getRetentionMillis() {
  String ret = getLoggingRetention();
  if (ret == "1w") {
    return 7 * 24 * 60 * 60 * 1000UL;  // 1 week
  } else if (ret == "1m") {
    return 30 * 24 * 60 * 60 * 1000UL; // 1 month
  } else if (ret == "3m") {
    return 90 * 24 * 60 * 60 * 1000UL; // 3 months
  }
  return 7 * 24 * 60 * 60 * 1000UL;   // Default 1 week
}