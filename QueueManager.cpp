#include "QueueManager.h"
#include <esp_heap_caps.h>

QueueManager* QueueManager::instance = nullptr;

QueueManager::QueueManager() : dataQueue(nullptr), queueMutex(nullptr) {}

QueueManager* QueueManager::getInstance() {
  if (instance == nullptr) {
    instance = new QueueManager();
  }
  return instance;
}

bool QueueManager::init() {
  // Create FreeRTOS queue for data points
  dataQueue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(char*));
  if (dataQueue == nullptr) {
    Serial.println("Failed to create data queue");
    return false;
  }
  
  // Create mutex for thread safety
  queueMutex = xSemaphoreCreateMutex();
  if (queueMutex == nullptr) {
    Serial.println("Failed to create queue mutex");
    return false;
  }
  
  Serial.println("QueueManager initialized successfully");
  return true;
}

bool QueueManager::enqueue(const JsonObject& dataPoint) {
  if (dataQueue == nullptr || queueMutex == nullptr) {
    return false;
  }
  
  // Take mutex
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  
  // Check if queue is full
  if (uxQueueMessagesWaiting(dataQueue) >= MAX_QUEUE_SIZE) {
    // Remove oldest item to make space
    char* oldItem;
    if (xQueueReceive(dataQueue, &oldItem, 0) == pdTRUE) {
      free(oldItem);
    }
  }
  
  // Serialize JSON to string
  String jsonString;
  serializeJson(dataPoint, jsonString);
  
  // Allocate memory for string in PSRAM
  char* jsonCopy = (char*)heap_caps_malloc(jsonString.length() + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (jsonCopy == nullptr) {
    // Fallback to internal RAM
    jsonCopy = (char*)malloc(jsonString.length() + 1);
    if (jsonCopy == nullptr) {
      xSemaphoreGive(queueMutex);
      return false;
    }
  }
  
  strcpy(jsonCopy, jsonString.c_str());
  
  // Add to queue
  bool success = xQueueSend(dataQueue, &jsonCopy, 0) == pdTRUE;
  
  if (success) {
    Serial.printf("Data queued: %s\n", dataPoint["name"].as<String>().c_str());
  } else {
    heap_caps_free(jsonCopy);
  }
  
  xSemaphoreGive(queueMutex);
  return success;
}

bool QueueManager::dequeue(JsonObject& dataPoint) {
  if (dataQueue == nullptr || queueMutex == nullptr) {
    return false;
  }
  
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  
  char* jsonString;
  bool success = false;
  
  if (xQueueReceive(dataQueue, &jsonString, 0) == pdTRUE) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error == DeserializationError::Ok) {
      JsonObject obj = doc.as<JsonObject>();
      for (JsonPair kv : obj) {
        dataPoint[kv.key()] = kv.value();
      }
      success = true;
    }
    
    heap_caps_free(jsonString);
  }
  
  xSemaphoreGive(queueMutex);
  return success;
}

bool QueueManager::peek(JsonObject& dataPoint) {
  if (dataQueue == nullptr || queueMutex == nullptr) {
    return false;
  }
  
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return false;
  }
  
  char* jsonString;
  bool success = false;
  
  if (xQueuePeek(dataQueue, &jsonString, 0) == pdTRUE) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error == DeserializationError::Ok) {
      JsonObject obj = doc.as<JsonObject>();
      for (JsonPair kv : obj) {
        dataPoint[kv.key()] = kv.value();
      }
      success = true;
    }
  }
  
  xSemaphoreGive(queueMutex);
  return success;
}

bool QueueManager::isEmpty() {
  if (dataQueue == nullptr) {
    return true;
  }
  return uxQueueMessagesWaiting(dataQueue) == 0;
}

bool QueueManager::isFull() {
  if (dataQueue == nullptr) {
    return false;
  }
  return uxQueueMessagesWaiting(dataQueue) >= MAX_QUEUE_SIZE;
}

int QueueManager::size() {
  if (dataQueue == nullptr) {
    return 0;
  }
  return uxQueueMessagesWaiting(dataQueue);
}

void QueueManager::clear() {
  if (dataQueue == nullptr || queueMutex == nullptr) {
    return;
  }
  
  if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }
  
  char* jsonString;
  while (xQueueReceive(dataQueue, &jsonString, 0) == pdTRUE) {
    heap_caps_free(jsonString);
  }
  
  xSemaphoreGive(queueMutex);
  Serial.println("Queue cleared");
}

void QueueManager::getStats(JsonObject& stats) {
  stats["size"] = size();
  stats["max_size"] = MAX_QUEUE_SIZE;
  stats["is_empty"] = isEmpty();
  stats["is_full"] = isFull();
}

QueueManager::~QueueManager() {
  clear();
  if (dataQueue) {
    vQueueDelete(dataQueue);
  }
  if (queueMutex) {
    vSemaphoreDelete(queueMutex);
  }
}