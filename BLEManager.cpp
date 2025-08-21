#include "BLEManager.h"
#include "CRUDHandler.h"
#include "QueueManager.h"
#include <esp_heap_caps.h>
#include <new>

BLEManager::BLEManager(const String& name, CRUDHandler* cmdHandler) 
  : serviceName(name), handler(cmdHandler), processing(false), streamTaskHandle(nullptr) {
  commandBuffer.reserve(COMMAND_BUFFER_SIZE);
  commandQueue = xQueueCreate(20, sizeof(String*));  // Increased queue size
}

BLEManager::~BLEManager() {
  stop();
  if (commandQueue) {
    vQueueDelete(commandQueue);
  }
}

bool BLEManager::begin() {
  // Initialize BLE
  BLEDevice::init(serviceName.c_str());
  
  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(this);
  
  // Create BLE Service
  pService = pServer->createService(SERVICE_UUID);
  
  // Create Command Characteristic (Write)
  pCommandChar = pService->createCharacteristic(
    COMMAND_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCommandChar->setCallbacks(this);
  
  // Create Response Characteristic (Notify)
  pResponseChar = pService->createCharacteristic(
    RESPONSE_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pResponseChar->addDescriptor(new BLE2902());
  
  // Start service
  pService->start();
  
  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  // Create command processing task with PSRAM stack
  xTaskCreatePinnedToCore(
    commandProcessingTask,
    "BLE_CMD_TASK",
    8192,  // Increased stack size for PSRAM usage
    this,
    1,
    &commandTaskHandle,
    1  // Pin to core 1
  );
  
  // Create streaming task
  xTaskCreatePinnedToCore(
    streamingTask,
    "BLE_STREAM_TASK",
    4096,
    this,
    1,
    &streamTaskHandle,
    1
  );
  
  Serial.println("BLE Manager initialized: " + serviceName);
  return true;
}

void BLEManager::stop() {
  if (commandTaskHandle) {
    vTaskDelete(commandTaskHandle);
    commandTaskHandle = nullptr;
  }
  
  if (streamTaskHandle) {
    vTaskDelete(streamTaskHandle);
    streamTaskHandle = nullptr;
  }
  
  if (pServer) {
    BLEDevice::deinit();
    Serial.println("BLE Manager stopped");
  }
}

void BLEManager::onConnect(BLEServer* pServer) {
  Serial.println("BLE Client connected");
}

void BLEManager::onDisconnect(BLEServer* pServer) {
  Serial.println("BLE Client disconnected");
  
  // Stop streaming when client disconnects
  extern CRUDHandler* crudHandler;
  if (crudHandler) {
    crudHandler->clearStreamDeviceId();
    QueueManager* queueMgr = QueueManager::getInstance();
    if (queueMgr) {
      queueMgr->clearStream();
    }
    Serial.println("Cleared streaming on disconnect");
  }
  
  BLEDevice::startAdvertising(); // Restart advertising
}

void BLEManager::onWrite(BLECharacteristic* pCharacteristic) {
  if (pCharacteristic == pCommandChar) {
    String value = pCharacteristic->getValue().c_str();
    receiveFragment(value);
  }
}

void BLEManager::receiveFragment(const String& fragment) {
  if (processing) return;
  
  if (fragment == "<END>") {
    processing = true;
    String* cmd = new String(commandBuffer);
    xQueueSend(commandQueue, &cmd, 0);
    commandBuffer = "";
    processing = false;
  } else {
    commandBuffer += fragment;
  }
}

void BLEManager::commandProcessingTask(void* parameter) {
  BLEManager* manager = static_cast<BLEManager*>(parameter);
  String* command;
  
  while (true) {
    if (xQueueReceive(manager->commandQueue, &command, portMAX_DELAY)) {
      manager->handleCompleteCommand(*command);
      delete command;
    }
  }
}

void BLEManager::handleCompleteCommand(const String& command) {
  Serial.printf("DEBUG: Raw JSON command: %s\n", command.c_str());
  
  // Allocate JSON document in PSRAM for large commands
  DynamicJsonDocument* doc = (DynamicJsonDocument*)heap_caps_malloc(sizeof(DynamicJsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!doc) {
    // Fallback to stack allocation
    DynamicJsonDocument stackDoc(2048);
    DeserializationError error = deserializeJson(stackDoc, command);
    if (error) {
      sendError("Invalid JSON: " + String(error.c_str()));
      return;
    }
    if (handler) {
      handler->handle(this, stackDoc);
    }
    return;
  }
  
  new(doc) DynamicJsonDocument(4096);  // Larger size for PSRAM
  DeserializationError error = deserializeJson(*doc, command);
  
  if (error) {
    sendError("Invalid JSON: " + String(error.c_str()));
    doc->~DynamicJsonDocument();
    heap_caps_free(doc);
    return;
  }
  
  if (handler) {
    handler->handle(this, *doc);
  } else {
    sendError("No handler configured");
  }
  
  // Clean up PSRAM allocation
  doc->~DynamicJsonDocument();
  heap_caps_free(doc);
}

void BLEManager::sendResponse(const JsonDocument& data) {
  String response;
  serializeJson(data, response);
  sendFragmented(response);
}

void BLEManager::sendError(const String& message) {
  DynamicJsonDocument doc(256);
  doc["status"] = "error";
  doc["message"] = message;
  sendResponse(doc);
}

void BLEManager::sendSuccess() {
  DynamicJsonDocument doc(128);
  doc["status"] = "ok";
  sendResponse(doc);
}

void BLEManager::sendFragmented(const String& data) {
  if (!pResponseChar) return;
  
  int i = 0;
  while (i < data.length()) {
    int end = i + CHUNK_SIZE;
    if (end > data.length()) {
      end = data.length();
    }
    
    String chunk = data.substring(i, end);
    pResponseChar->setValue(chunk.c_str());
    pResponseChar->notify();
    
    vTaskDelay(pdMS_TO_TICKS(FRAGMENT_DELAY_MS));
    i = end;
  }
  
  // Send end marker
  pResponseChar->setValue("<END>");
  pResponseChar->notify();
}

void BLEManager::streamingTask(void* parameter) {
  BLEManager* manager = static_cast<BLEManager*>(parameter);
  QueueManager* queueMgr = QueueManager::getInstance();
  
  Serial.println("BLE Streaming task started");
  int loopCount = 0;
  
  while (true) {
    if (queueMgr && !queueMgr->isStreamEmpty()) {
      DynamicJsonDocument dataDoc(512);
      JsonObject dataPoint = dataDoc.to<JsonObject>();
      
      if (queueMgr->dequeueStream(dataPoint)) {
        Serial.println("Streaming data via BLE");
        DynamicJsonDocument response(512);
        response["status"] = "data";
        response["data"] = dataPoint;
        manager->sendResponse(response);
      }
    } else {
      loopCount++;
      if (loopCount % 100 == 0) {
        Serial.printf("Stream queue empty, loop count: %d\n", loopCount);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}