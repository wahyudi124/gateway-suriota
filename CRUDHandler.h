#ifndef CRUD_HANDLER_H
#define CRUD_HANDLER_H

#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "ServerConfig.h"
#include "LoggingConfig.h"

class BLEManager; // Forward declaration

class CRUDHandler {
private:
  ConfigManager* configManager;
  ServerConfig* serverConfig;
  LoggingConfig* loggingConfig;
  
  // Operation handlers
  void handleRead(BLEManager* manager, const String& type, const JsonDocument& command);
  void handleCreate(BLEManager* manager, const String& type, const JsonDocument& command);
  void handleUpdate(BLEManager* manager, const String& type, const JsonDocument& command);
  void handleDelete(BLEManager* manager, const String& type, const JsonDocument& command);

public:
  CRUDHandler(ConfigManager* config, ServerConfig* serverCfg, LoggingConfig* loggingCfg);
  
  void handle(BLEManager* manager, const JsonDocument& command);
};

#endif