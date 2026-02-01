#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <map>
#include <vector>
#include <website.h>

BLECharacteristic* pScoreChar = nullptr;
BLECharacteristic* pDataCharacteristic = nullptr;
bool pwaConnected = false;

unsigned long lastNotifyTime = 0;
const unsigned long NOTIFY_COOLDOWN_MS = 100;
bool isNotifying = false;
std::map<std::string, String> senderMessageBuffers;

std::map<std::string, std::string> teamToSender = {
  {"HOME", "ESP32-BLE-Sender-1"},
  {"AWAY", "ESP32-BLE-Sender-2"}
};

// Halftime management - Async state machine
enum HalftimeStep {
  HALFTIME_IDLE,
  HALFTIME_START_SAVE_SCORES,
  HALFTIME_START_REQUEST_DATA,
  HALFTIME_START_WAIT_DATA,
  HALFTIME_START_SWAP_SENDERS,
  HALFTIME_START_SEND_DATA,
  HALFTIME_START_RESET_TIMER,
  HALFTIME_END_REQUEST_DATA,
  HALFTIME_END_WAIT_DATA,
  HALFTIME_END_SAVE_SCORES,
  HALFTIME_END_RESET_SCORES,
  HALFTIME_END_SWAP_BACK,
  HALFTIME_END_RESET_TIMER
};

struct HalftimeState {
  HalftimeStep currentStep = HALFTIME_IDLE;
  bool isHalftime = false;
  bool sendersSwapped = false;
  int savedHomeScore = 0;
  int savedAwayScore = 0;
  int savedHomeAttempts = 0;
  int savedAwayAttempts = 0;
  unsigned long stepStartTime = 0;
  bool processingHalftime = false;
};

HalftimeState halftimeState;

struct PendingScoreSync {
  bool pending = false;
  std::string senderName;
  int score = 0;
  int attempts = 0;
};

volatile bool gSyncPending = false;
PendingScoreSync gSync;

struct SenderDevice {
  std::string name;
  BLEAddress address;
  BLEClient* client;
  BLERemoteCharacteristic* txCharacteristic;
  BLERemoteCharacteristic* rxCharacteristic;
  bool connected;
  int rssi;
  String lastData;
  unsigned long lastUpdate;
  unsigned long lastDataTime;

  SenderDevice()
    : address(BLEAddress("00:00:00:00:00:00"))
    , client(nullptr)
    , txCharacteristic(nullptr)
    , rxCharacteristic(nullptr)
    , connected(false)
    , rssi(0)
    , lastUpdate(0)
    , lastDataTime(0) {
  }
};

struct ConnectionHealth {
  unsigned long lastDataTime;
  unsigned long lastPingTime;
  int missedPings;
  bool needsReconnect;
};

std::map<std::string, SenderDevice*> senderDevices;
std::map<std::string, ConnectionHealth> connectionHealthMap;
std::map<BLERemoteCharacteristic*, std::string> characteristicToSenderMap;

BLEServer* pServer = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;
bool oldPwaConnected = false;

std::vector<std::string> targetSenders = {"ESP32-BLE-Sender-1", "ESP32-BLE-Sender-2"};

void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi);
void sendDeviceListToPWA();
void forceReconnect(const std::string& deviceName);
void cleanupStaleConnections();
void monitorConnections();
bool syncScoreToSender(const std::string& senderName, int newScore, int newAttempts);
void processSenderData(const std::string& senderName, const String& jsonData);
void requestDataFromSenders();

void sendScoreToPWA() {
  if (!pDataCharacteristic || !pwaConnected) {
    return;
  }
  
  unsigned long now = millis();
  if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
    return;
  }
  
  String matchTime = getFormattedTime();
  String jsonData =
    "{"
    "\"From\":\"CentralUnit\","
    "\"Time\":\"" + matchTime + "\","
    "\"Score\":[" + String(homeScore) + "," + String(awayScore) + "],"
    "\"GoalAttempt\":[" + String(homeAttempts) + "," + String(awayAttempts) + "]"
    "}";
  
  isNotifying = true;
  pDataCharacteristic->setValue((uint8_t*)jsonData.c_str(), jsonData.length());
  pDataCharacteristic->notify(true);
  isNotifying = false;
  lastNotifyTime = millis();
}

void sendScoreToDisplays() {
  if (!pScoreChar) {
    Serial.println("ScoreBoard BLE not initialized");
    return;
  }
  
  String matchTime = getFormattedTime();
  String jsonData =
    "{"
    "\"From\":\"CentralUnit\","
    "\"Time\":\"" + matchTime + "\","
    "\"Score\":[" + String(homeScore) + "," + String(awayScore) + "],"
    "\"GoalAttempt\":[" + String(homeAttempts) + "," + String(awayAttempts) + "]"
    "}";
  
  pScoreChar->setValue((uint8_t*)jsonData.c_str(), jsonData.length());
  pScoreChar->notify(true);
}

void sendScoreNew() {
  static int lastHomeScore = -1;
  static int lastAwayScore = -1;
  
  if (homeScore == lastHomeScore && awayScore == lastAwayScore) {
    return;
  }
  
  lastHomeScore = homeScore;
  lastAwayScore = awayScore;
  
  sendScoreToPWA();
  sendScoreToDisplays();
}

class GatewayServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    pwaConnected = true;
    Serial.println("PWA Connected");
    String status = "connected";
    pStatusCharacteristic->setValue(status.c_str());
    pStatusCharacteristic->notify();
    sendScoreNew();
  }

  void onDisconnect(BLEServer* pServer) {
    pwaConnected = false;
    Serial.println("PWA Disconnected");
    String status = "disconnected";
    pStatusCharacteristic->setValue(status.c_str());
    delay(500);
    pServer->startAdvertising();
    Serial.println("Advertising restarted for PWA");
  }
};

bool syncScoreToSender(const std::string& senderName, int newScore, int newAttempts) {
  auto itDev = senderDevices.find(senderName);
  if (itDev == senderDevices.end()) {
    return false;
  }

  SenderDevice* sender = itDev->second;
  if (!sender->connected || sender->rxCharacteristic == nullptr) {
    return false;
  }

  String command = "{\"command\":\"set_score\",\"score\":" + String(newScore) + ",\"pogingen\":" + String(newAttempts) + "}";
  const int chunkSize = 20;
  bool success = true;
  Serial.println("Syncing score to sender: " + String(senderName.c_str()) + " Score: " + String(newScore) + " Attempts: " + String(newAttempts));
  for (int i = 0; i < command.length(); i += chunkSize) {
    int len = min(chunkSize, (int)(command.length() - i));
    if (i > 0) {
      delay(50);
    }
    
    bool ok = sender->rxCharacteristic->writeValue((uint8_t*)(command.c_str() + i), len, false);
    if (!ok) {
      success = false;
      break;
    }
    delay(10);
  }

  return success;
}

void requestDataFromSenders() {
  for (auto& devicePair : senderDevices) {
    std::string senderName = devicePair.first;
    SenderDevice* sender = devicePair.second;
    
    if (!sender->connected || !sender->rxCharacteristic) {
      continue;
    }
    
    String command = "{\"command\":\"get_data\"}";
    const int chunkSize = 20;
    bool success = true;
    
    for (int i = 0; i < command.length(); i += chunkSize) {
      int len = min(chunkSize, (int)(command.length() - i));
      if (i > 0) {
        delay(50);
      }
      
      bool ok = sender->rxCharacteristic->writeValue((uint8_t*)(command.c_str() + i), len, false);
      if (!ok) {
        success = false;
        break;
      }
      delay(10);
    }
  }
}

// Halftime functions
bool sendResetToSender(const std::string& senderName) {
  auto itDev = senderDevices.find(senderName);
  if (itDev == senderDevices.end()) {
    Serial.println("Sender not found for reset: " + String(senderName.c_str()));
    return false;
  }

  SenderDevice* sender = itDev->second;
  if (!sender->connected || sender->rxCharacteristic == nullptr) {
    Serial.println("Sender not connected for reset: " + String(senderName.c_str()));
    return false;
  }

  String command = "{\"command\":\"reset_score\"}";
  const int chunkSize = 20;
  bool success = true;

  Serial.println("Sending reset to: " + String(senderName.c_str()));
  
  for (int i = 0; i < command.length(); i += chunkSize) {
    int len = min(chunkSize, (int)(command.length() - i));
    if (i > 0) {
      delay(50);
    }
    
    bool ok = sender->rxCharacteristic->writeValue((uint8_t*)(command.c_str() + i), len, false);
    if (!ok) {
      success = false;
      Serial.println("Failed to send reset chunk");
      break;
    }
    delay(10);
  }

  return success;
}

void swapSenderAssignments() {
  Serial.println("Swapping sender assignments...");
  
  // Swap the mapping
  std::string sender1 = teamToSender["HOME"];
  std::string sender2 = teamToSender["AWAY"];
  
  teamToSender["HOME"] = sender2;
  teamToSender["AWAY"] = sender1;
  
  halftimeState.sendersSwapped = !halftimeState.sendersSwapped;
  
  Serial.println("HOME now mapped to: " + String(teamToSender["HOME"].c_str()));
  Serial.println("AWAY now mapped to: " + String(teamToSender["AWAY"].c_str()));
}

void saveCurrentScores() {
  halftimeState.savedHomeScore = homeScore;
  halftimeState.savedAwayScore = awayScore;
  halftimeState.savedHomeAttempts = homeAttempts;
  halftimeState.savedAwayAttempts = awayAttempts;
  
  Serial.println("Scores saved - HOME: " + String(homeScore) + " AWAY: " + String(awayScore));
}

void sendSwappedDataToSenders() {
  Serial.println("Sending swapped data to senders...");
  
  // After swapping, HOME is now mapped to what was AWAY sender
  // So we send the AWAY data to the new HOME sender (which is physically sender 2)
  std::string homeSender = teamToSender["HOME"];
  std::string awaySender = teamToSender["AWAY"];
  
  // Use SAVED scores to avoid race conditions with incoming sender data
  Serial.println("Using saved scores - HOME: " + String(halftimeState.savedHomeScore) + " AWAY: " + String(halftimeState.savedAwayScore));
  
  // Send away score to HOME sender (because they swapped)
  if (!syncScoreToSender(awaySender, halftimeState.savedAwayScore, halftimeState.savedAwayAttempts)) {
    Serial.println("Failed to sync away score to HOME sender");
  } else {
    Serial.println("Synced away score (" + String(halftimeState.savedAwayScore) + ") to HOME sender");
  }
  
  // Send home score to AWAY sender (because they swapped)
  if (!syncScoreToSender(homeSender, halftimeState.savedHomeScore, halftimeState.savedHomeAttempts)) {
    Serial.println("Failed to sync home score to AWAY sender");
  } else {
    Serial.println("Synced home score (" + String(halftimeState.savedHomeScore) + ") to AWAY sender");
  }
}

void resetAllScoresAndSenders() {
  Serial.println("Resetting all scores and senders...");
  
  // Reset gateway scores
  homeScore = 0;
  awayScore = 0;
  homeAttempts = 0;
  awayAttempts = 0;
  
  // Send reset to both senders
  for (auto& devicePair : senderDevices) {
    sendResetToSender(devicePair.first);
  }
  
  // Notify PWA
  sendScoreNew();
  
  Serial.println("All scores reset");
}

// Trigger halftime process - non-blocking
void triggerHalftime(bool isHalftime) {
  if (halftimeState.processingHalftime) {
    Serial.println("Halftime already in progress, ignoring command");
    return;
  }
  
  Serial.println("=== HALFTIME COMMAND RECEIVED ===");
  Serial.println("Value: " + String(isHalftime ? "true" : "false"));
  
  halftimeState.processingHalftime = true;
  halftimeState.stepStartTime = millis();
  
  if (isHalftime) {
    Serial.println("Initiating halftime start procedure...");
    halftimeState.currentStep = HALFTIME_START_SAVE_SCORES;
  } else {
    Serial.println("Initiating match end procedure...");
    halftimeState.currentStep = HALFTIME_END_REQUEST_DATA;
  }
}

// Process halftime state machine - call from main loop
void processHalftimeStateMachine() {
  if (!halftimeState.processingHalftime) {
    return;
  }
  
  unsigned long now = millis();
  unsigned long elapsed = now - halftimeState.stepStartTime;
  
  switch (halftimeState.currentStep) {
    case HALFTIME_IDLE:
      // Nothing to do
      break;
      
    case HALFTIME_START_SAVE_SCORES:
      Serial.println("Step 1: Saving current scores BEFORE requesting data...");
      saveCurrentScores();
      Serial.println("Saved - HOME: " + String(halftimeState.savedHomeScore) + " AWAY: " + String(halftimeState.savedAwayScore));
      halftimeState.isHalftime = true;
      halftimeState.currentStep = HALFTIME_START_REQUEST_DATA;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_START_REQUEST_DATA:
      Serial.println("Step 2: Requesting data from senders...");
      requestDataFromSenders();
      halftimeState.currentStep = HALFTIME_START_WAIT_DATA;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_START_WAIT_DATA:
      if (elapsed > 800) {  // Wait 800ms for data
        halftimeState.currentStep = HALFTIME_START_SWAP_SENDERS;
        halftimeState.stepStartTime = now;
      }
      break;
      
    case HALFTIME_START_SWAP_SENDERS:
      Serial.println("Step 3: Swapping sender assignments...");
      swapSenderAssignments();
      halftimeState.currentStep = HALFTIME_START_SEND_DATA;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_START_SEND_DATA:
      if (elapsed > 100) {  // Small delay between steps
        Serial.println("Step 4: Sending swapped data to senders...");
        sendSwappedDataToSenders();
        halftimeState.currentStep = HALFTIME_START_RESET_TIMER;
        halftimeState.stepStartTime = now;
      }
      break;
      
    case HALFTIME_START_RESET_TIMER:
      if (elapsed > 200) {  // Wait for data to be sent
        Serial.println("Step 5: Resetting timer...");
        timerReset();
        sendScoreToPWA();
        Serial.println("=== HALFTIME STARTED - Senders swapped, timer reset ===");
        halftimeState.currentStep = HALFTIME_IDLE;
        halftimeState.processingHalftime = false;
      }
      break;
      
    case HALFTIME_END_REQUEST_DATA:
      Serial.println("Step 1: Requesting final data from senders...");
      requestDataFromSenders();
      halftimeState.currentStep = HALFTIME_END_WAIT_DATA;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_END_WAIT_DATA:
      if (elapsed > 800) {  // Wait 800ms for data
        halftimeState.currentStep = HALFTIME_END_SAVE_SCORES;
        halftimeState.stepStartTime = now;
      }
      break;
      
    case HALFTIME_END_SAVE_SCORES:
      Serial.println("Step 2: Saving final scores...");
      saveCurrentScores();
      Serial.println("Final Score - HOME: " + String(homeScore) + " AWAY: " + String(awayScore));
      halftimeState.currentStep = HALFTIME_END_RESET_SCORES;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_END_RESET_SCORES:
      Serial.println("Step 3: Resetting all scores...");
      resetAllScoresAndSenders();
      halftimeState.currentStep = HALFTIME_END_SWAP_BACK;
      halftimeState.stepStartTime = now;
      break;
      
    case HALFTIME_END_SWAP_BACK:
      if (elapsed > 200) {  // Wait for reset to complete
        if (halftimeState.sendersSwapped) {
          Serial.println("Step 4: Swapping senders back to original...");
          swapSenderAssignments();
        }
        halftimeState.isHalftime = false;
        halftimeState.currentStep = HALFTIME_END_RESET_TIMER;
        halftimeState.stepStartTime = now;
      }
      break;
      
    case HALFTIME_END_RESET_TIMER:
      if (elapsed > 100) {
        Serial.println("Step 5: Resetting timer...");
        timerReset();
        halftimeState.savedHomeScore = 0;
        halftimeState.savedAwayScore = 0;
        halftimeState.savedHomeAttempts = 0;
        halftimeState.savedAwayAttempts = 0;
        Serial.println("=== MATCH ENDED - Everything reset, senders restored ===");
        halftimeState.currentStep = HALFTIME_IDLE;
        halftimeState.processingHalftime = false;
      }
      break;
  }
}

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String raw = pCharacteristic->getValue();
    String msg = String(raw.c_str());
    msg.trim();

    if (msg.length() == 0) return;

    if (msg.startsWith("{")) {
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, msg);
      if (err) return;

      const char* command = doc["Command"];
      if (!command) return;

      String cmd = String(command);
      cmd.toUpperCase();

      if (cmd == "CHANGE NAMES") {
        JsonArray values = doc["Value"];
        if (!values || values.size() != 2) return;

        String newHome = values[0].as<String>();
        String newAway = values[1].as<String>();
        changeNames(newHome, newAway);
        return;
      } else if (cmd == "TIME") {
        String action = "";
        if (doc["Value"].is<JsonArray>()) {
          JsonArray values = doc["Value"];
          if (values.size() < 1) return;
          action = values[0].as<String>();
        } else if (doc["Value"].is<const char*>() || doc["Value"].is<String>()) {
          action = doc["Value"].as<String>();
        } else {
          return;
        }
        
        action.toUpperCase();
        if (action == "START") {
          timerStart();
        } else if (action == "STOP") {
          timerStop();
        } else if (action == "RESET") {
          timerReset();
        } else {
          return;
        }
        
        sendScoreToPWA();
        return;
      } else if (cmd == "SCORE") {
        JsonArray values = doc["Value"];
        if (!values || values.size() < 2) return;

        String teamStr = values[0].as<String>();
        String opStr = values[1].as<String>();
        teamStr.toUpperCase();
        opStr.toUpperCase();

        int* scorePtr = nullptr;
        if (teamStr == "HOME") {
          scorePtr = &homeScore;
        } else if (teamStr == "AWAY") {
          scorePtr = &awayScore;
        } else {
          return;
        }

        if (opStr == "INCREMENT") {
          (*scorePtr)++;
        } else if (opStr == "DECREMENT") {
          (*scorePtr)--;
        } else if (opStr == "SET") {
          if (values.size() >= 3) {
            *scorePtr = values[2].as<int>();
          } else {
            return;
          }
        } else {
          return;
        }

        *scorePtr = constrain(*scorePtr, 0, 99);
        sendScoreNew();

        std::string teamKey = teamStr.c_str();
        auto it = teamToSender.find(teamKey);
        if (it != teamToSender.end()) {
          const std::string& senderName = it->second;
          int attempts = (teamStr == "HOME") ? homeAttempts : awayAttempts;
          
          Serial.println("Syncing " + teamStr + " (score: " + String(*scorePtr) + ") to sender: " + String(senderName.c_str()));
          
          gSync.senderName = senderName;
          gSync.score = *scorePtr;
          gSync.attempts = attempts;
          gSync.pending = true;
          gSyncPending = true;
        }
        return;
      } else if (cmd == "HALFTIME") {
        // Handle halftime - Value should be boolean
        bool halftimeValue = false;
        
        if (doc["Value"].is<bool>()) {
          halftimeValue = doc["Value"].as<bool>();
        } else if (doc["Value"].is<int>()) {
          halftimeValue = (doc["Value"].as<int>() != 0);
        } else if (doc["Value"].is<const char*>() || doc["Value"].is<String>()) {
          String valStr = doc["Value"].as<String>();
          valStr.toLowerCase();
          halftimeValue = (valStr == "true" || valStr == "1");
        } else {
          Serial.println("Invalid HALFTIME value type");
          return;
        }
        
        triggerHalftime(halftimeValue);
        return;
      }
    }

    if (msg == "get_devices") {
      sendDeviceListToPWA();
    } else if (msg == "hello") {
      timerReset();
    }
  }
};

void sendDeviceListToPWA() {
  if (!pwaConnected || !pDataCharacteristic) {
    return;
  }

  unsigned long now = millis();
  if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
    return;
  }

  String deviceList = "[";
  bool first = true;
  unsigned long currentTime = millis();

  for (auto& devicePair : senderDevices) {
    if (!first) deviceList += ",";
    first = false;

    unsigned long timeSinceUpdate = currentTime - devicePair.second->lastUpdate;
    String status = devicePair.second->connected ? "connected" : "disconnected";
    if (devicePair.second->connected && timeSinceUpdate > 10000) {
      status = "weak";
    }

    deviceList += "{";
    deviceList += "\"name\":\"" + String(devicePair.first.c_str()) + "\",";
    deviceList += "\"status\":\"" + status + "\",";
    deviceList += "\"rssi\":" + String(devicePair.second->rssi) + ",";
    deviceList += "\"data\":\"" + devicePair.second->lastData + "\",";
    deviceList += "\"lastUpdate\":" + String(timeSinceUpdate);
    deviceList += "}";
  }

  deviceList += "]";

  isNotifying = true;
  pDataCharacteristic->setValue(deviceList.c_str());
  pDataCharacteristic->notify();
  isNotifying = false;
  lastNotifyTime = millis();
}

void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi) {
  if (!pwaConnected || !pDataCharacteristic) {
    return;
  }

  unsigned long now = millis();
  if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
    return;
  }

  String jsonData = "{";
  jsonData += "\"sender\":\"" + String(senderName.c_str()) + "\",";
  jsonData += "\"data\":\"" + data + "\",";
  jsonData += "\"rssi\":" + String(rssi) + ",";
  jsonData += "\"timestamp\":" + String(millis());
  jsonData += "}";

  isNotifying = true;
  pDataCharacteristic->setValue(jsonData.c_str());
  pDataCharacteristic->notify();
  isNotifying = false;
  lastNotifyTime = millis();
}

bool isTargetSender(const std::string& deviceName) {
  for (const auto& target : targetSenders) {
    if (deviceName == target) {
      return true;
    }
  }
  return false;
}

static void senderNotificationCallback(BLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  std::string senderName = "Unknown";
  if (characteristicToSenderMap.find(pRemoteCharacteristic) != characteristicToSenderMap.end()) {
    senderName = characteristicToSenderMap[pRemoteCharacteristic];
  } else {
    for (auto& devPair : senderDevices) {
      if (devPair.second->txCharacteristic == pRemoteCharacteristic) {
        senderName = devPair.first;
        characteristicToSenderMap[pRemoteCharacteristic] = senderName;
        break;
      }
    }
  }

  if (senderName == "Unknown") return;

  unsigned long currentTime = millis();
  if (connectionHealthMap.find(senderName) == connectionHealthMap.end()) {
    ConnectionHealth health;
    health.lastDataTime = currentTime;
    health.lastPingTime = currentTime;
    health.missedPings = 0;
    health.needsReconnect = false;
    connectionHealthMap[senderName] = health;
  } else {
    connectionHealthMap[senderName].lastDataTime = currentTime;
    connectionHealthMap[senderName].missedPings = 0;
    connectionHealthMap[senderName].needsReconnect = false;
  }

  int rssi = -99;
  if (senderDevices.find(senderName) != senderDevices.end()) {
    rssi = senderDevices[senderName]->rssi;
    senderDevices[senderName]->lastDataTime = currentTime;
  }

  String receivedData = "";
  for (int i = 0; i < length; i++) {
    receivedData += (char)pData[i];
  }
  
  if (receivedData.indexOf("\"status\":\"ping\"") != -1) {
    return;
  }
  
  if (senderMessageBuffers.find(senderName) == senderMessageBuffers.end()) {
    senderMessageBuffers[senderName] = "";
  }
  
  senderMessageBuffers[senderName] += receivedData;
  String& buffer = senderMessageBuffers[senderName];
  
  int start = buffer.indexOf('{');
  int end = buffer.lastIndexOf('}');
  
  if (start != -1 && end != -1 && end > start) {
    String completeJson = buffer.substring(start, end + 1);
    processSenderData(senderName, completeJson);
    
    if (end + 1 < buffer.length()) {
      buffer = buffer.substring(end + 1);
    } else {
      buffer = "";
    }
  }
  
  if (buffer.length() > 500) {
    buffer = "";
  }
}

void processSenderData(const std::string& senderName, const String& jsonData) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonData);
  if (err) return;
  
  if (doc["score"].is<int>()) {
    String scoreStr = String(doc["score"].as<int>());
    int attempts = 0;
    
    if (doc["pogingen"].is<int>()) {
      attempts = doc["pogingen"].as<int>();
    }
    
    if (senderDevices.find(senderName) != senderDevices.end()) {
      senderDevices[senderName]->lastData = scoreStr;
      senderDevices[senderName]->lastUpdate = millis();
      senderDevices[senderName]->lastDataTime = millis();
    }
    
    int rssi = -99;
    if (senderDevices.find(senderName) != senderDevices.end()) {
      rssi = senderDevices[senderName]->rssi;
    }
    
    sendSensorDataToPWA(senderName, scoreStr, rssi);
    
    // Determine which team this sender belongs to based on current mapping
    int sender = 0;
    if (teamToSender["HOME"] == senderName) {
      sender = 1; // HOME team
      Serial.println("Data from HOME sender: " + String(senderName.c_str()));
    } else if (teamToSender["AWAY"] == senderName) {
      sender = 2; // AWAY team
      Serial.println("Data from AWAY sender: " + String(senderName.c_str()));
    } else {
      // Unknown sender, skip
      Serial.println("Unknown sender mapping: " + String(senderName.c_str()));
      return;
    }
    
    scoreChange(scoreStr.toInt(), sender, attempts);
  }
}

void forceReconnect(const std::string& deviceName) {
  if (senderDevices.find(deviceName) == senderDevices.end()) {
    return;
  }

  SenderDevice* device = senderDevices[deviceName];
  if (device->client) {
    if (device->client->isConnected()) {
      device->client->disconnect();
    }
    delete device->client;
    device->client = nullptr;
  }

  device->connected = false;
  device->txCharacteristic = nullptr;
  device->rxCharacteristic = nullptr;

  for (auto it = characteristicToSenderMap.begin(); it != characteristicToSenderMap.end();) {
    if (it->second == deviceName) {
      it = characteristicToSenderMap.erase(it);
    } else {
      ++it;
    }
  }

  if (connectionHealthMap.find(deviceName) != connectionHealthMap.end()) {
    connectionHealthMap[deviceName].missedPings = 0;
    connectionHealthMap[deviceName].needsReconnect = true;
    connectionHealthMap[deviceName].lastDataTime = millis();
  }
}

void connectToSender(BLEAdvertisedDevice* device) {
  std::string deviceName = device->getName().c_str();

  if (senderDevices.find(deviceName) != senderDevices.end() && senderDevices[deviceName]->connected) {
    if (millis() - senderDevices[deviceName]->lastDataTime < 10000) {
      senderDevices[deviceName]->rssi = device->getRSSI();
      senderDevices[deviceName]->lastUpdate = millis();
      return;
    } else {
      forceReconnect(deviceName);
    }
  }

  BLEClient* pClient = BLEDevice::createClient();
  bool connected = false;
  unsigned long connectStart = millis();

  while (millis() - connectStart < 5000) {
    if (pClient->connect(device)) {
      connected = true;
      break;
    }
    delay(100);
  }

  if (!connected) {
    delete pClient;
    return;
  }

  delay(100);
  BLERemoteService* pRemoteService = pClient->getService(SENDER_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    delete pClient;
    return;
  }

  BLERemoteCharacteristic* pRemoteTX = pRemoteService->getCharacteristic(SENDER_CHAR_UUID_TX);
  if (pRemoteTX == nullptr) {
    pClient->disconnect();
    delete pClient;
    return;
  }

  BLERemoteCharacteristic* pRemoteRX = pRemoteService->getCharacteristic(SENDER_CHAR_UUID_RX);
  if (pRemoteRX == nullptr) {
    pClient->disconnect();
    delete pClient;
    return;
  }

  SenderDevice* sender;
  if (senderDevices.find(deviceName) != senderDevices.end()) {
    sender = senderDevices[deviceName];
    if (sender->client && sender->client != pClient) {
      delete sender->client;
    }
  } else {
    sender = new SenderDevice();
  }

  sender->name = deviceName;
  sender->address = device->getAddress();
  sender->client = pClient;
  sender->txCharacteristic = pRemoteTX;
  sender->rxCharacteristic = pRemoteRX;
  sender->connected = true;
  sender->rssi = device->getRSSI();
  sender->lastUpdate = millis();
  sender->lastDataTime = millis();
  sender->lastData = "0";

  senderDevices[deviceName] = sender;
  characteristicToSenderMap[pRemoteTX] = deviceName;

  ConnectionHealth health;
  health.lastDataTime = millis();
  health.lastPingTime = millis();
  health.missedPings = 0;
  health.needsReconnect = false;
  connectionHealthMap[deviceName] = health;

  if (pRemoteTX->canNotify()) {
    pRemoteTX->registerForNotify(senderNotificationCallback);
    uint8_t cccdValue[] = {0x01, 0x00};
    BLERemoteDescriptor* pCCCD = pRemoteTX->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (pCCCD != nullptr) {
      pCCCD->writeValue(cccdValue, 2, true);
    }
  }

  senderMessageBuffers[deviceName] = "";
  delay(200);
  
  String command = "{\"command\":\"get_data\"}";
  const int chunkSize = 20;
  bool success = true;
  
  for (int i = 0; i < command.length(); i += chunkSize) {
    int len = min(chunkSize, (int)(command.length() - i));
    if (i > 0) {
      delay(50);
    }
    
    bool ok = pRemoteRX->writeValue((uint8_t*)(command.c_str() + i), len, false);
    if (!ok) {
      success = false;
      break;
    }
    delay(10);
  }
}

void scanForSenders() {
  BLEDevice::stopAdvertising();
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  BLEScanResults* foundDevices = pBLEScan->start(3, false);

  int targetFound = 0;
  int connected = 0;

  for (int i = 0; i < foundDevices->getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices->getDevice(i);
    if (device.haveName()) {
      std::string deviceName = device.getName().c_str();
      if (isTargetSender(deviceName)) {
        targetFound++;
        if (connectionHealthMap.find(deviceName) != connectionHealthMap.end() && connectionHealthMap[deviceName].needsReconnect) {
          forceReconnect(deviceName);
        }
        connectToSender(&device);
        if (senderDevices.find(deviceName) != senderDevices.end() && senderDevices[deviceName]->connected) {
          connected++;
        }
      }
    }
  }

  pBLEScan->clearResults();
  BLEDevice::startAdvertising();
}

void monitorConnections() {
  unsigned long currentTime = millis();
  for (auto& devicePair : senderDevices) {
    std::string name = devicePair.first;
    SenderDevice* device = devicePair.second;
    if (device->connected) {
      if (connectionHealthMap.find(name) == connectionHealthMap.end()) {
        ConnectionHealth health;
        health.lastDataTime = currentTime;
        health.lastPingTime = currentTime;
        health.missedPings = 0;
        health.needsReconnect = false;
        connectionHealthMap[name] = health;
      }
      ConnectionHealth& health = connectionHealthMap[name];
      unsigned long timeSinceData = currentTime - health.lastDataTime;
      if (timeSinceData > 15000) {
        health.missedPings++;
        if (health.missedPings > 2) {
          health.needsReconnect = true;
          device->connected = false;
          if (pwaConnected) {
            sendDeviceListToPWA();
          }
        }
      }
    }
  }
}

void cleanupStaleConnections() {
  unsigned long currentTime = millis();
  for (auto it = senderDevices.begin(); it != senderDevices.end();) {
    SenderDevice* device = it->second;
    if (!device->connected && (currentTime - device->lastUpdate > 60000)) {
      if (device->client) {
        if (device->client->isConnected()) {
          device->client->disconnect();
        }
        delete device->client;
      }
      delete device;
      for (auto cit = characteristicToSenderMap.begin(); cit != characteristicToSenderMap.end();) {
        if (cit->second == it->first) {
          cit = characteristicToSenderMap.erase(cit);
        } else {
          ++cit;
        }
      }
      connectionHealthMap.erase(it->first);
      it = senderDevices.erase(it);
    } else {
      ++it;
    }
  }
}

void setupScoreBoardBLE() {
  if (!pServer) {
    return;
  }
  
  BLEService* pScoreService = pServer->createService(SCOREBOARD_SERVICE_UUID);
  if (!pScoreService) {
    return;
  }
  
  pScoreChar = pScoreService->createCharacteristic(
    SCOREBOARD_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  
  if (!pScoreChar) {
    return;
  }
  
  pScoreChar->addDescriptor(new BLE2902());
  pScoreService->start();
}

void setupBLEGateway() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(512);
  esp_power_level_t power = ESP_PWR_LVL_P9;
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, power);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new GatewayServerCallbacks());
  BLEService* pService = pServer->createService(GATEWAY_SERVICE_UUID);

  pDataCharacteristic = pService->createCharacteristic(
    GATEWAY_CHAR_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pDataCharacteristic->addDescriptor(new BLE2902());

  pStatusCharacteristic = pService->createCharacteristic(
    GATEWAY_CHAR_STATUS_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pStatusCharacteristic->addDescriptor(new BLE2902());

  pCommandCharacteristic = pService->createCharacteristic(GATEWAY_CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCommandCharacteristic->setCallbacks(new CommandCallbacks());

  pService->start();
  setupScoreBoardBLE();

  BLEAdvertising* pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(GATEWAY_SERVICE_UUID);
  pAdvertising->addServiceUUID(SCOREBOARD_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();
}

void bleInit() {
  Serial.println("ESP32 BLE Gateway - Score Forwarder");
  setupBLEGateway();
  scanForSenders();
}

void handleBluetooth() {
  if (!pwaConnected && oldPwaConnected) {
    oldPwaConnected = pwaConnected;
  }

  if (pwaConnected && !oldPwaConnected) {
    oldPwaConnected = pwaConnected;
    sendDeviceListToPWA();
    sendScoreNew();
  }

  static unsigned long lastMonitorTime = 0;
  if (millis() - lastMonitorTime >= 5000) {
    monitorConnections();
    lastMonitorTime = millis();
  }

  static unsigned long lastScanTime = 0;
  if (millis() - lastScanTime >= 30000) {
    scanForSenders();
    lastScanTime = millis();
  }

  static unsigned long lastCleanupTime = 0;
  if (millis() - lastCleanupTime >= 30000) {
    cleanupStaleConnections();
    lastCleanupTime = millis();
  }

  static unsigned long lastDeviceListTime = 0;
  if (pwaConnected && millis() - lastDeviceListTime >= 10000) {
    sendDeviceListToPWA();
    lastDeviceListTime = millis();
  }

  static unsigned long lastTimerUpdateTime = 0;
  static bool timerZeroHandled = false;
  if (pwaConnected && millis() - lastTimerUpdateTime >= 1000) {
    sendScoreToPWA();
    lastTimerUpdateTime = millis();
    
    uint32_t remainingMs = getRemainingTimerMs();
    if (remainingMs == 0 && !timerZeroHandled) {
      requestDataFromSenders();
      timerZeroHandled = true;
    } else if (remainingMs > 0) {
      timerZeroHandled = false;
    }
  }

  if (gSyncPending && gSync.pending) {
    gSyncPending = false;
    gSync.pending = false;
    syncScoreToSender(gSync.senderName, gSync.score, gSync.attempts);
  }
  
  // Process halftime state machine
  processHalftimeStateMachine();
}