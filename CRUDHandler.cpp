#include "CRUDHandler.h"
#include "BLEManager.h"

CRUDHandler::CRUDHandler(ConfigManager* config, ServerConfig* serverCfg, LoggingConfig* loggingCfg) 
  : configManager(config), serverConfig(serverCfg), loggingConfig(loggingCfg) {}

void CRUDHandler::handle(BLEManager* manager, const JsonDocument& command) {
  String op = command["op"] | "";
  String type = command["type"] | "";
  
  if (op == "read") {
    handleRead(manager, type, command);
  } else if (op == "create") {
    handleCreate(manager, type, command);
  } else if (op == "update") {
    handleUpdate(manager, type, command);
  } else if (op == "delete") {
    handleDelete(manager, type, command);
  } else {
    manager->sendError("Unsupported operation: " + op);
  }
}

void CRUDHandler::handleRead(BLEManager* manager, const String& type, const JsonDocument& command) {
  if (type == "devices") {
    DynamicJsonDocument response(512);
    response["status"] = "ok";
    JsonArray devices = response.createNestedArray("devices");
    configManager->listDevices(devices);
    manager->sendResponse(response);
    
  } else if (type == "devices_summary") {
    DynamicJsonDocument response(1024);
    response["status"] = "ok";
    JsonArray summary = response.createNestedArray("devices_summary");
    configManager->getDevicesSummary(summary);
    manager->sendResponse(response);
    
  } else if (type == "device") {
    String deviceId = command["device_id"] | "";
    DynamicJsonDocument response(1024);
    response["status"] = "ok";
    JsonObject data = response.createNestedObject("data");
    if (configManager->readDevice(deviceId, data)) {
      manager->sendResponse(response);
    } else {
      manager->sendError("Device not found");
    }
    
  } else if (type == "registers") {
    String deviceId = command["device_id"] | "";
    DynamicJsonDocument response(1024);
    response["status"] = "ok";
    JsonArray registers = response.createNestedArray("registers");
    if (configManager->listRegisters(deviceId, registers)) {
      manager->sendResponse(response);
    } else {
      manager->sendError("No registers found");
    }
    
  } else if (type == "registers_summary") {
    String deviceId = command["device_id"] | "";
    DynamicJsonDocument response(1024);
    response["status"] = "ok";
    JsonArray summary = response.createNestedArray("registers_summary");
    if (configManager->getRegistersSummary(deviceId, summary)) {
      manager->sendResponse(response);
    } else {
      manager->sendError("No registers found");
    }
    
  } else if (type == "server_config") {
    DynamicJsonDocument response(2048);
    response["status"] = "ok";
    JsonObject serverConfigObj = response.createNestedObject("server_config");
    if (serverConfig->getConfig(serverConfigObj)) {
      manager->sendResponse(response);
    } else {
      manager->sendError("Failed to get server config");
    }
    
  } else if (type == "logging_config") {
    DynamicJsonDocument response(512);
    response["status"] = "ok";
    JsonObject loggingConfigObj = response.createNestedObject("logging_config");
    if (loggingConfig->getConfig(loggingConfigObj)) {
      manager->sendResponse(response);
    } else {
      manager->sendError("Failed to get logging config");
    }
    
  } else {
    manager->sendError("Unsupported read type: " + type);
  }
}

void CRUDHandler::handleCreate(BLEManager* manager, const String& type, const JsonDocument& command) {
  if (type == "device") {
    JsonObjectConst config = command["config"];
    String deviceId = configManager->createDevice(config);
    if (!deviceId.isEmpty()) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["device_id"] = deviceId;
      manager->sendResponse(response);
    } else {
      manager->sendError("Device creation failed");
    }
    
  } else if (type == "register") {
    String deviceId = command["device_id"] | "";
    JsonObjectConst config = command["config"];
    String registerId = configManager->createRegister(deviceId, config);
    if (!registerId.isEmpty()) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["register_id"] = registerId;
      manager->sendResponse(response);
    } else {
      manager->sendError("Register creation failed");
    }
    
  } else {
    manager->sendError("Unsupported create type: " + type);
  }
}

void CRUDHandler::handleUpdate(BLEManager* manager, const String& type, const JsonDocument& command) {
  if (type == "server_config") {
    JsonObjectConst config = command["config"];
    if (serverConfig->updateConfig(config)) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["message"] = "Server configuration updated";
      manager->sendResponse(response);
    } else {
      manager->sendError("Server configuration update failed");
    }
    
  } else if (type == "logging_config") {
    JsonObjectConst config = command["config"];
    if (loggingConfig->updateConfig(config)) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["message"] = "Logging configuration updated";
      manager->sendResponse(response);
    } else {
      manager->sendError("Logging configuration update failed");
    }
    
  } else {
    manager->sendError("Unsupported update type: " + type);
  }
}

void CRUDHandler::handleDelete(BLEManager* manager, const String& type, const JsonDocument& command) {
  if (type == "device") {
    String deviceId = command["device_id"] | "";
    if (configManager->deleteDevice(deviceId)) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["message"] = "Device deleted";
      manager->sendResponse(response);
    } else {
      manager->sendError("Device deletion failed");
    }
    
  } else if (type == "register") {
    String deviceId = command["device_id"] | "";
    String registerId = command["register_id"] | "";
    if (configManager->deleteRegister(deviceId, registerId)) {
      DynamicJsonDocument response(128);
      response["status"] = "ok";
      response["message"] = "Register deleted";
      manager->sendResponse(response);
    } else {
      manager->sendError("Register deletion failed");
    }
    
  } else {
    manager->sendError("Unsupported delete type: " + type);
  }
}