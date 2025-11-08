#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <SD.h>
#include <SPI.h>
#include "RTCManager.h"
#include "LoggingConfig.h"

#define SD_CS   11
#define SD_MOSI 10
#define SD_SCK  12
#define SD_MISO 13

class SDLogger {
private:
  LoggingConfig* loggingConfig;
  RTCManager* rtcManager;
  bool initialized;
  unsigned long lastLogTime;
  unsigned long logInterval;
  String batchBuffer;
  int batchCount;
  
  bool initSD();
  void parseInterval(const String& interval);
  String getLogFileName();
  void cleanupOldLogs();
  void flushBatch();
  String formatLogEntry(const String& registerName, const String& value);
  
public:
  SDLogger(LoggingConfig* config, RTCManager* rtc);
  
  bool begin();
  void logRegisterData(const String& registerName, float value);
  void logRegisterData(const String& registerName, int32_t value);
  void logRegisterData(const String& registerName, uint32_t value);
  bool shouldLog();
  void updateLastLogTime();
  void forceBatchFlush();
  
  ~SDLogger();
};

#endif