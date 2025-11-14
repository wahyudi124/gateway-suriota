#include "SDLogger.h"

SDLogger::SDLogger(LoggingConfig* config, RTCManager* rtc) 
  : loggingConfig(config), rtcManager(rtc), initialized(false), lastLogTime(0), logInterval(300000), batchCount(0) {
  batchBuffer.reserve(2048);
}

bool SDLogger::begin() {
  if (!initSD()) {
    Serial.println("SD Card initialization failed");
    return false;
  }
  
  if (loggingConfig) {
    parseInterval(loggingConfig->getLoggingInterval());
  }
  
  initialized = true;
  Serial.println("SDLogger initialized successfully");
  return true;
}

bool SDLogger::initSD() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed");
    return false;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }
  
  Serial.printf("SD Card Type: %s\n", 
    cardType == CARD_MMC ? "MMC" :
    cardType == CARD_SD ? "SDSC" :
    cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  return true;
}

void SDLogger::parseInterval(const String& interval) {
  if (interval == "5m") {
    logInterval = 5 * 60 * 1000;  // 5 minutes
  } else if (interval == "10m") {
    logInterval = 10 * 60 * 1000; // 10 minutes
  } else if (interval == "30m") {
    logInterval = 30 * 60 * 1000; // 30 minutes
  } else {
    logInterval = 5 * 60 * 1000;  // Default 5 minutes
  }
}

bool SDLogger::shouldLog() {
  if (!initialized) return false;
  
  unsigned long currentTime = millis();
  if (currentTime - lastLogTime >= logInterval) {
    return true;
  }
  return false;
}

void SDLogger::updateLastLogTime() {
  lastLogTime = millis();
}

String SDLogger::getLogFileName() {
  if (rtcManager) {
    DateTime now = rtcManager->getCurrentTime();
    return String("/logs/") + String(now.year()) + "-" + 
           String(now.month()) + "-" + String(now.day()) + ".csv";
  }
  return "/logs/data.csv";
}

String SDLogger::formatLogEntry(const String& registerName, const String& value) {
  String timestamp;
  if (rtcManager) {
    DateTime now = rtcManager->getCurrentTime();
    timestamp = String(now.unixtime());
  } else {
    timestamp = String(millis());
  }
  return timestamp + "," + registerName + "," + value;
}

void SDLogger::flushBatch() {
  if (batchBuffer.length() == 0) return;
  
  String filename = getLogFileName();
  
  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
  }
  
  bool fileExists = SD.exists(filename);
  File logFile = SD.open(filename, FILE_APPEND);
  if (logFile) {
    if (!fileExists) {
      logFile.println("Timestamp,RegisterName,Value");
    }
    logFile.print(batchBuffer);
    logFile.close();
    Serial.printf("Logged %d entries to SD\n", batchCount);
  }
  
  batchBuffer = "";
  batchCount = 0;
  updateLastLogTime();
}

void SDLogger::logRegisterData(const String& registerName, float value) {
  if (!initialized) return;
  
  String logEntry = formatLogEntry(registerName, String(value, 6));
  batchBuffer += logEntry + "\n";
  batchCount++;
  
  if (shouldLog() || batchCount >= 50) {
    flushBatch();
  }
}

void SDLogger::logRegisterData(const String& registerName, int32_t value) {
  if (!initialized) return;
  
  String logEntry = formatLogEntry(registerName, String(value));
  batchBuffer += logEntry + "\n";
  batchCount++;
  
  if (shouldLog() || batchCount >= 50) {
    flushBatch();
  }
}

void SDLogger::logRegisterData(const String& registerName, uint32_t value) {
  if (!initialized) return;
  
  String logEntry = formatLogEntry(registerName, String(value));
  batchBuffer += logEntry + "\n";
  batchCount++;
  
  if (shouldLog() || batchCount >= 50) {
    flushBatch();
  }
}

void SDLogger::cleanupOldLogs() {
  if (!initialized || !loggingConfig) return;
  
  unsigned long retentionMillis = loggingConfig->getRetentionMillis();
  unsigned long currentTime = millis();
  
  File root = SD.open("/logs");
  if (!root || !root.isDirectory()) {
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String fileName = file.name();
      unsigned long fileTime = file.getLastWrite() * 1000; // Convert to millis
      
      if (currentTime - fileTime > retentionMillis) {
        String fullPath = "/logs/" + fileName;
        SD.remove(fullPath);
        Serial.printf("Deleted old log file: %s\n", fullPath.c_str());
      }
    }
    file = root.openNextFile();
  }
  root.close();
}

void SDLogger::forceBatchFlush() {
  if (batchCount > 0) {
    flushBatch();
  }
}

SDLogger::~SDLogger() {
  forceBatchFlush(); // Flush any remaining data
  SD.end();
}