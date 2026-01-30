#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <map>
#include <vector>
#include <website.h>

// Define BLE variables here
BLECharacteristic* pScoreChar = nullptr;
BLECharacteristic* pDataCharacteristic = nullptr;
bool pwaConnected = false;

// Notification throttling to prevent BLE stack errors
unsigned long lastNotifyTime = 0;
const unsigned long NOTIFY_COOLDOWN_MS = 100;  // Minimum time between notifications
bool isNotifying = false;

std::map<std::string, std::string> teamToSender
    = { { "HOME", "ESP32-BLE-Sender-1" }, { "AWAY", "ESP32-BLE-Sender-2" } };

struct PendingScoreSync {
    bool pending = false;
    std::string senderName;
    int score = 0;
};

volatile bool gSyncPending = false;
PendingScoreSync gSync;

// Structure for connected sender devices
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
        , lastDataTime(0)
    {
    }
};

// Connection health structure
struct ConnectionHealth {
    unsigned long lastDataTime;
    unsigned long lastPingTime;
    int missedPings;
    bool needsReconnect;
};

// Global variables
std::map<std::string, SenderDevice*> senderDevices;
std::map<std::string, ConnectionHealth> connectionHealthMap;
std::map<BLERemoteCharacteristic*, std::string> characteristicToSenderMap;

BLEServer* pServer = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;
bool oldPwaConnected = false;

// Target sender names
std::vector<std::string> targetSenders = { "ESP32-BLE-Sender-1", "ESP32-BLE-Sender-2" };

// Forward declarations
void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi);
void sendDeviceListToPWA();
void forceReconnect(const std::string& deviceName);
void cleanupStaleConnections();
void monitorConnections();
bool syncScoreToSender(const std::string& senderName, int newScore);

// NEW FUNCTION: Send score to PWA
void sendScoreToPWA() {
    if (!pDataCharacteristic || !pwaConnected) {

        return;  // Silently skip if not connected
    }
    
    // Throttle notifications to prevent BLE stack overload
    unsigned long now = millis();
    if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
        return;  // Skip if too soon after last notification
    }
    
    String matchTime = getFormattedTime();  // Get time in mm:ss:msms format
    String jsonData =
        "{"
        "\"From\":\"CentralUnit\","
        "\"Time\":\"" + matchTime + "\","
        "\"Score\":[" + String(homeScore) + "," + String(awayScore) + "],"
        "\"GoalAttempt\":[54,48]"
        "}";
    
    isNotifying = true;
    pDataCharacteristic->setValue((uint8_t*)jsonData.c_str(), jsonData.length());
    pDataCharacteristic->notify(true);
    isNotifying = false;
    lastNotifyTime = millis();
    
    Serial.print("Score sent to PWA: ");
    Serial.println(jsonData);
}

// NEW FUNCTION: Send score to displays (LED scoreboards)
void sendScoreToDisplays() {
    if (!pScoreChar) {
        Serial.println("ScoreBoard BLE not initialized");
        return;
    }
    
    String matchTime = getFormattedTime();  // Get time in mm:ss:msms format
    String jsonData =
        "{"
        "\"From\":\"CentralUnit\","
        "\"Time\":\"" + matchTime + "\","
        "\"Score\":[" + String(homeScore) + "," + String(awayScore) + "],"
        "\"GoalAttempt\":[54,48]"
        "}";
    
    pScoreChar->setValue((uint8_t*)jsonData.c_str(), jsonData.length());
    pScoreChar->notify(true);
    
    Serial.print("Score sent to displays: ");
    Serial.println(jsonData);
}

// REPLACEMENT for the old sendScoreNew in scoreData.cpp
void sendScoreNew() {
    static int lastHomeScore = -1;
    static int lastAwayScore = -1;
    
    // Check if score actually changed
    if (homeScore == lastHomeScore && awayScore == lastAwayScore) {
        return;
    }
    
    lastHomeScore = homeScore;
    lastAwayScore = awayScore;
    
    Serial.print("Score update detected: ");
    Serial.print(homeScore);
    Serial.print(" - ");
    Serial.println(awayScore);
    
    // Send to PWA
    sendScoreToPWA();
    
    // Send to displays
    sendScoreToDisplays();
}

// Callback for PWA connection events
class GatewayServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer)
    {
        pwaConnected = true;
        Serial.println("PWA Connected!");

        String status = "connected";
        pStatusCharacteristic->setValue(status.c_str());
        pStatusCharacteristic->notify();
        
        // Send current score when PWA connects
        sendScoreNew();
    }

    void onDisconnect(BLEServer* pServer)
    {
        pwaConnected = false;
        Serial.println("PWA Disconnected!");

        String status = "disconnected";
        pStatusCharacteristic->setValue(status.c_str());
        // No need to notify on disconnect - client is already gone

        delay(500);
        pServer->startAdvertising();
        Serial.println("Advertising restarted for PWA");
    }
};

// Stuur score sync commando naar sender
bool syncScoreToSender(const std::string& senderName, int newScore)
{
    auto itDev = senderDevices.find(senderName);
    if (itDev == senderDevices.end()) {
        Serial.print("Sender not connected: ");
        Serial.println(senderName.c_str());
        return false;
    }

    SenderDevice* sender = itDev->second;

    if (!sender->connected || sender->rxCharacteristic == nullptr) {
        Serial.print("Sender not ready for commands: ");
        Serial.println(senderName.c_str());
        return false;
    }

    // Build JSON command
    String command = "{\"command\":\"set_score\",\"score\":" + String(newScore) + "}";
    
    // DEBUG: Print the full command
    Serial.print("Full command to send: ");
    Serial.println(command);
    Serial.print("Command length: ");
    Serial.println(command.length());
    
    const int chunkSize = 20;
    bool success = true;

    for (int i = 0; i < command.length(); i += chunkSize) {
        int len = min(chunkSize, (int)(command.length() - i));
        
        // Add delay between chunks (except first)
        if (i > 0) {
            delay(50); // Increased delay
        }
        
        bool ok = sender->rxCharacteristic->writeValue(
            (uint8_t*)(command.c_str() + i),
            len,
            false
        );
        
        if (!ok) {
            Serial.println("Failed to send chunk");
            success = false;
            break;
        }
        
        Serial.print("Sent chunk ");
        Serial.print(i / chunkSize + 1);
        Serial.print("/");
        Serial.println((command.length() + chunkSize - 1) / chunkSize);
        
        delay(10); // Small delay after sending
    }

    // Send score update to PWA and displays
    if (success) {
        sendScoreNew();
        Serial.println("Score sync sent and broadcasted");
    } else {
        Serial.println("Score sync failed");
    }

    return success;
}

// Callback for commands from PWA
class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) override
    {
        String raw = pCharacteristic->getValue();
        String msg = String(raw.c_str());
        msg.trim();

        if (msg.length() == 0)
            return;

        Serial.print("Command from PWA: ");
        Serial.println(msg);

        if (msg.startsWith("{")) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, msg);

            if (err) {
                Serial.print("JSON parse failed: ");
                Serial.println(err.c_str());
                return;
            }

            const char* command = doc["Command"];
            if (!command) {
                Serial.println("JSON missing 'Command'");
                return;
            }

            String cmd = String(command);
            cmd.toUpperCase();

            if (cmd == "CHANGE NAMES") {
                JsonArray values = doc["Value"];

                if (!values || values.size() != 2) {
                    Serial.println("CHANGE NAMES requires Value[2]");
                    return;
                }

                String newHome = values[0].as<String>();
                String newAway = values[1].as<String>();

                Serial.print("Changing names → ");
                Serial.print(newHome);
                Serial.print(" vs ");
                Serial.println(newAway);

                changeNames(newHome, newAway);
                return;
            } else if (cmd == "TIME") {
                // Handle both string and array formats for Value
                String action = "";
                
                if (doc["Value"].is<JsonArray>()) {
                    JsonArray values = doc["Value"];
                    if (values.size() < 1) {
                        Serial.println("TIME requires Value");
                        return;
                    }
                    action = values[0].as<String>();
                } else if (doc["Value"].is<const char*>() || doc["Value"].is<String>()) {
                    action = doc["Value"].as<String>();
                } else {
                    Serial.println("TIME requires Value as string or array");
                    return;
                }
                
                action.toUpperCase();
                
                if (action == "START") {
                    timerStart();
                    Serial.println("Timer STARTED");
                } else if (action == "STOP") {
                    timerStop();
                    Serial.println("Timer STOPPED");
                } else if (action == "RESET") {
                    timerReset();
                    Serial.println("Timer RESET");
                } else {
                    Serial.print("Invalid TIME action: ");
                    Serial.println(action);
                    return;
                }
                
                // Send updated time to PWA immediately
                sendScoreToPWA();
                return;
            } else if (cmd == "SCORE") {
                JsonArray values = doc["Value"];

                if (!values || values.size() < 2) {
                    Serial.println("SCORE requires Value[2]");
                    return;
                }

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
                    Serial.println("Invalid team (must be HOME or AWAY)");
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
                        Serial.println("SET requires Value[3]");
                        return;
                    }
                } else {
                    Serial.println("Invalid SCORE operation");
                    return;
                }

                *scorePtr = constrain(*scorePtr, 0, 99);

                Serial.print("SCORE updated → ");
                Serial.print(teamStr);
                Serial.print(" = ");
                Serial.println(*scorePtr);

                // Trigger score update to PWA and displays
                sendScoreNew();

                std::string teamKey = teamStr.c_str();
                auto it = teamToSender.find(teamKey);
                if (it != teamToSender.end()) {
                    const std::string& senderName = it->second;

                    Serial.print("Syncing score to sender: ");
                    Serial.println(senderName.c_str());

                    gSync.senderName = senderName;
                    gSync.score = *scorePtr;
                    gSync.pending = true;
                    gSyncPending = true;

                    Serial.println("Queued score sync (will send in loop)");
                } else {
                    Serial.print("No sender mapping found for team: ");
                    Serial.println(teamStr);
                }

                return;
            }
        }

        if (msg == "get_devices") {
            sendDeviceListToPWA();
        } else if (msg == "reset") {
            Serial.println("Reset command received");
        } else if (msg == "hello") {
            timerReset();
        }
    }
};

// Send device list to PWA
void sendDeviceListToPWA()
{
    if (!pwaConnected || !pDataCharacteristic) {
        return;
    }

    // Throttle notifications to prevent BLE stack overload
    unsigned long now = millis();
    if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
        return;  // Skip if too soon after last notification
    }

    String deviceList = "[";
    bool first = true;
    unsigned long currentTime = millis();

    for (auto& devicePair : senderDevices) {
        if (!first)
            deviceList += ",";
        first = false;

        unsigned long timeSinceUpdate = currentTime - devicePair.second->lastUpdate;
        String status = devicePair.second->connected ? "connected" : "disconnected";

        // If connected but no data for a while, mark as weak
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

        // Serial.print("Device ");
        // Serial.print(devicePair.first.c_str());
        // Serial.print(" - Status: ");
        // Serial.print(status);
        // Serial.print(", Data: ");
        // Serial.println(devicePair.second->lastData);
    }

    deviceList += "]";

    isNotifying = true;
    pDataCharacteristic->setValue(deviceList.c_str());
    pDataCharacteristic->notify();
    isNotifying = false;
    lastNotifyTime = millis();

    // Serial.print("Sent device list to PWA: ");
    // Serial.println(deviceList);
}

// Send sensor data to PWA
void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi)
{
    if (!pwaConnected || !pDataCharacteristic) {
        return;  // Silently skip if not connected
    }

    // Throttle notifications to prevent BLE stack overload
    unsigned long now = millis();
    if (isNotifying || (now - lastNotifyTime) < NOTIFY_COOLDOWN_MS) {
        return;  // Skip if too soon after last notification
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

    Serial.print("To PWA: ");
    Serial.println(jsonData);
}

// Check if device is a target sender
bool isTargetSender(const std::string& deviceName)
{
    for (const auto& target : targetSenders) {
        if (deviceName == target) {
            return true;
        }
    }
    return false;
}

// Helper function to extract score from possibly broken JSON
String extractScoreFromData(String data)
{
    int scorePos = data.indexOf("\"score\":");

    if (scorePos != -1) {
        int colonPos = scorePos + 8;

        int endPos = -1;
        for (int i = colonPos; i < data.length(); i++) {
            char c = data.charAt(i);
            if (c == ',' || c == '}' || c == ' ' || c == '"') {
                endPos = i;
                break;
            }
        }

        if (endPos != -1) {
            String scoreStr = data.substring(colonPos, endPos);
            scoreStr.trim();
            return scoreStr;
        }
    }

    data.trim();
    for (int i = 0; i < data.length(); i++) {
        if (!isdigit(data.charAt(i))) {
            String digits = "";
            for (int j = 0; j < data.length(); j++) {
                if (isdigit(data.charAt(j))) {
                    digits += data.charAt(j);
                }
            }
            if (digits.length() > 0) {
                return digits;
            }
            return "0";
        }
    }

    return data;
}

// Notification callback with ping handling
static void senderNotificationCallback(
    BLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
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

    if (senderName == "Unknown") {
        Serial.println("Unknown sender for notification");
        return;
    }

    // Update connection health
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

    // Check if this is a ping (ignore ping messages)
    if (receivedData.indexOf("\"status\":\"ping\"") != -1) {
        // Serial.print("Ping from ");
        // Serial.println(senderName.c_str());
        return;
    }

    Serial.print("From ");
    Serial.print(senderName.c_str());
    Serial.print(" (RSSI: ");
    Serial.print(rssi);
    Serial.print(" dBm): ");
    Serial.println(receivedData.c_str());

    String scoreStr = extractScoreFromData(receivedData);

    if (senderDevices.find(senderName) != senderDevices.end()) {
        senderDevices[senderName]->lastData = scoreStr;
        senderDevices[senderName]->lastUpdate = currentTime;
        senderDevices[senderName]->lastDataTime = currentTime;

        Serial.print("Updated ");
        Serial.print(senderName.c_str());
        Serial.print(" score to: ");
        Serial.println(scoreStr);
    }

    int sender = 0;
    if (senderName == "ESP32-BLE-Sender-1") {
        sender = 1;
    } else if (senderName == "ESP32-BLE-Sender-2") {
        sender = 2;
    }

    sendSensorDataToPWA(senderName, scoreStr, rssi);
    scoreChange(scoreStr.toInt(), sender);
}

// Force reconnect to a sender
void forceReconnect(const std::string& deviceName)
{
    if (senderDevices.find(deviceName) == senderDevices.end()) {
        return;
    }

    SenderDevice* device = senderDevices[deviceName];

    Serial.print("Forcing reconnection for ");
    Serial.println(deviceName.c_str());

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

    Serial.print("Cleaned up connection for ");
    Serial.println(deviceName.c_str());
}

// Connect to a sender device
void connectToSender(BLEAdvertisedDevice* device)
{
    std::string deviceName = device->getName().c_str();

    Serial.print("Connecting to sender: ");
    Serial.println(deviceName.c_str());

    if (senderDevices.find(deviceName) != senderDevices.end() && senderDevices[deviceName]->connected) {

        if (millis() - senderDevices[deviceName]->lastDataTime < 10000) {
            Serial.print("Already connected to ");
            Serial.println(deviceName.c_str());
            senderDevices[deviceName]->rssi = device->getRSSI();
            senderDevices[deviceName]->lastUpdate = millis();
            return;
        } else {
            Serial.print("Stale connection to ");
            Serial.print(deviceName.c_str());
            Serial.println(" - forcing reconnection");
            forceReconnect(deviceName);
        }
    }

    BLEClient* pClient = BLEDevice::createClient();

    Serial.print("   Attempting connection to: ");
    Serial.println(device->getAddress().toString().c_str());

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
        Serial.println("Connection timeout");
        delete pClient;
        return;
    }

    Serial.println("Connected to device");
    delay(100);

    BLERemoteService* pRemoteService = pClient->getService(SENDER_SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found");
        pClient->disconnect();
        delete pClient;
        return;
    }

    Serial.println("Service found");

    BLERemoteCharacteristic* pRemoteTX = pRemoteService->getCharacteristic(SENDER_CHAR_UUID_TX);
    if (pRemoteTX == nullptr) {
        Serial.println("TX characteristic not found");
        pClient->disconnect();
        delete pClient;
        return;
    }

    Serial.println("TX characteristic found");

    BLERemoteCharacteristic* pRemoteRX = pRemoteService->getCharacteristic(SENDER_CHAR_UUID_RX);
    if (pRemoteRX == nullptr) {
        Serial.println("RX characteristic not found");
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
        Serial.println("Registering for notifications...");

        pRemoteTX->registerForNotify(senderNotificationCallback);

        uint8_t cccdValue[] = { 0x01, 0x00 };
        BLERemoteDescriptor* pCCCD = pRemoteTX->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pCCCD != nullptr) {
            if (pCCCD->writeValue(cccdValue, 2, true)) {
                Serial.println("Notifications enabled");
            } else {
                Serial.println("Failed to enable notifications");
            }
        }
    }

    std::string value = pRemoteTX->readValue().c_str();
    if (!value.empty()) {
        sender->lastData = String(value.c_str());
        Serial.print("Initial value: ");
        Serial.println(value.c_str());
    }

    Serial.print("Successfully connected to sender: ");
    Serial.println(deviceName.c_str());

    if (pwaConnected) {
        sendDeviceListToPWA();
    }
}

// Scan for sender devices
void scanForSenders()
{
    Serial.println("\nScanning for sender devices...");
    Serial.println("Temporarily pausing PWA advertising for scan...");

    BLEDevice::stopAdvertising();

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    BLEScanResults* foundDevices = pBLEScan->start(3, false);

    Serial.print("Found ");
    Serial.print(foundDevices->getCount());
    Serial.println(" total devices");

    int targetFound = 0;
    int connected = 0;

    for (int i = 0; i < foundDevices->getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices->getDevice(i);

        if (device.haveName()) {
            std::string deviceName = device.getName().c_str();

            if (isTargetSender(deviceName)) {
                targetFound++;

                Serial.print("Target found: ");
                Serial.print(deviceName.c_str());
                Serial.print(" (RSSI: ");
                Serial.print(device.getRSSI());
                Serial.print(" dBm) - ");

                if (connectionHealthMap.find(deviceName) != connectionHealthMap.end()
                    && connectionHealthMap[deviceName].needsReconnect) {
                    Serial.println("Needs reconnection...");
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
    Serial.println("Restarted PWA advertising");

    Serial.print("\nScan Summary: ");
    Serial.print(targetFound);
    Serial.print(" target(s) found, ");
    Serial.print(connected);
    Serial.println(" connected");
}

// Monitor connection health
void monitorConnections()
{
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
                    Serial.print("No data from ");
                    Serial.print(name.c_str());
                    Serial.print(" for ");
                    Serial.print(timeSinceData / 1000);
                    Serial.println(" seconds, marking for reconnection");

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

// Clean up stale connections
void cleanupStaleConnections()
{
    unsigned long currentTime = millis();

    for (auto it = senderDevices.begin(); it != senderDevices.end();) {
        SenderDevice* device = it->second;

        if (!device->connected && (currentTime - device->lastUpdate > 60000)) {
            Serial.print("Removing stale device: ");
            Serial.println(it->first.c_str());

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

// Setup ScoreBoard BLE
void setupScoreBoardBLE() {
    Serial.println("Setting up ScoreBoard BLE Service...");
    
    if (!pServer) {
        Serial.println("ERROR: BLE Server not initialized for ScoreBoard!");
        return;
    }
    
    BLEService* pScoreService = pServer->createService(SCOREBOARD_SERVICE_UUID);
    
    if (!pScoreService) {
        Serial.println("ERROR: Failed to create ScoreBoard service!");
        return;
    }
    
    pScoreChar = pScoreService->createCharacteristic(
        SCOREBOARD_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    if (!pScoreChar) {
        Serial.println("ERROR: Failed to create ScoreBoard characteristic!");
        return;
    }
    
    pScoreChar->addDescriptor(new BLE2902());
    pScoreService->start();
    
    Serial.println("ScoreBoard BLE Service started");
}

// Setup BLE Gateway
void setupBLEGateway()
{
    Serial.println("Setting up BLE Gateway...");

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

    pCommandCharacteristic
        = pService->createCharacteristic(GATEWAY_CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    pService->start();

    // Setup ScoreBoard service for displays
    setupScoreBoardBLE();

    // Setup advertising for BOTH services
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(GATEWAY_SERVICE_UUID);
    pAdvertising->addServiceUUID(SCOREBOARD_SERVICE_UUID);  // Add ScoreBoard service
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x40);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);

    BLEDevice::startAdvertising();

    Serial.println("\nBLE Gateway READY for PWA");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
    Serial.print("Gateway Service UUID: ");
    Serial.println(GATEWAY_SERVICE_UUID);
    Serial.print("ScoreBoard Service UUID: ");
    Serial.println(SCOREBOARD_SERVICE_UUID);
}

void bleInit()
{
    Serial.println("\n==========================================");
    Serial.println("ESP32 BLE Gateway - Score Forwarder");
    Serial.println("==========================================");
    Serial.println("Mode: Forwarding actual scores from senders");
    Serial.println("Target senders:");
    for (const auto& target : targetSenders) {
        Serial.print("  • ");
        Serial.println(target.c_str());
    }
    Serial.println("==========================================\n");

    setupBLEGateway();
    scanForSenders();
}

void handleBluetooth()
{
    if (!pwaConnected && oldPwaConnected) {
        oldPwaConnected = pwaConnected;
    }

    if (pwaConnected && !oldPwaConnected) {
        oldPwaConnected = pwaConnected;
        sendDeviceListToPWA();
        // Send initial score when PWA connects
        sendScoreNew();
    }

    // Monitor connection health every 5 seconds
    static unsigned long lastMonitorTime = 0;
    if (millis() - lastMonitorTime >= 5000) {
        monitorConnections();
        lastMonitorTime = millis();
    }

    // Periodic scan for senders (every 30 seconds)
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime >= 30000) {
        scanForSenders();
        lastScanTime = millis();
    }

    // Cleanup stale connections every 30 seconds
    static unsigned long lastCleanupTime = 0;
    if (millis() - lastCleanupTime >= 30000) {
        cleanupStaleConnections();
        lastCleanupTime = millis();
    }

    // Send device list to PWA periodically (every 10 seconds when connected)
    static unsigned long lastDeviceListTime = 0;
    if (pwaConnected && millis() - lastDeviceListTime >= 10000) {
        sendDeviceListToPWA();
        lastDeviceListTime = millis();
    }

    // Send timer updates to PWA periodically (every 1 second when connected)
    static unsigned long lastTimerUpdateTime = 0;
    if (pwaConnected && millis() - lastTimerUpdateTime >= 1000) {
        sendScoreToPWA();  // This sends score AND timer
        lastTimerUpdateTime = millis();
    }

    // Handle score sync to senders
    if (gSyncPending && gSync.pending) {
        gSyncPending = false;
        gSync.pending = false;

        Serial.print("Sending queued score sync to ");
        Serial.print(gSync.senderName.c_str());
        Serial.print(" score=");
        Serial.println(gSync.score);

        syncScoreToSender(gSync.senderName, gSync.score);
    }
}