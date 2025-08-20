#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

class QueueManager {
private:
  static QueueManager* instance;
  QueueHandle_t dataQueue;
  SemaphoreHandle_t queueMutex;
  static const int MAX_QUEUE_SIZE = 100;
  
  QueueManager();

public:
  static QueueManager* getInstance();
  
  bool init();
  bool enqueue(const JsonObject& dataPoint);
  bool dequeue(JsonObject& dataPoint);
  bool peek(JsonObject& dataPoint);
  bool isEmpty();
  bool isFull();
  int size();
  void clear();
  void getStats(JsonObject& stats);
  
  ~QueueManager();
};

#endif