#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <website.h>
#include <ArduinoJson.h>

std::map<std::string, std::string> teamToSender = {
    {"HOME", "ESP32-BLE-Sender-1"},
    {"AWAY", "ESP32-BLE-Sender-2"}
};
struct PendingScoreSync {
  bool pending = false;
  std::string senderName;
  int score = 0;
};

volatile bool gSyncPending = false;   // simpele flag
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

    SenderDevice()
        : address(BLEAddress("00:00:00:00:00:00"))
        , client(nullptr)
        , txCharacteristic(nullptr)
        , rxCharacteristic(nullptr)
        , connected(false)
        , rssi(0)
        , lastUpdate(0)
    {
    }
};

// Global variables
std::map<std::string, SenderDevice*> senderDevices;
BLEServer* pServer = nullptr;
BLECharacteristic* pDataCharacteristic = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;
bool pwaConnected = false;
bool oldPwaConnected = false;

// Target sender names
std::vector<std::string> targetSenders = { "ESP32-BLE-Sender-1", "ESP32-BLE-Sender-2" };

// Forward declarations
void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi);
void sendDeviceListToPWA();

// Global map for notification callbacks
std::map<BLERemoteCharacteristic*, std::string> characteristicToSenderMap;

// Callback for PWA connection events
class GatewayServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer){
        pwaConnected = true;
        Serial.println("PWA Connected!");

        // Update status characteristic
        String status = "connected";
        pStatusCharacteristic->setValue(status.c_str());
        pStatusCharacteristic->notify();
    }

    void onDisconnect(BLEServer* pServer)
    {
        pwaConnected = false;
        Serial.println("PWA Disconnected!");

        // Update status characteristic
        String status = "disconnected";
        pStatusCharacteristic->setValue(status.c_str());
        pStatusCharacteristic->notify();

        // Restart advertising
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
        Serial.print("❌ Sender not connected: ");
        Serial.println(senderName.c_str());
        return false;
    }

    SenderDevice* sender = itDev->second;

    if (!sender->connected || sender->rxCharacteristic == nullptr) {
        Serial.print("❌ Sender not ready for commands: ");
        Serial.println(senderName.c_str());
        return false;
    }

    // Check write capability
    bool canWrite     = sender->rxCharacteristic->canWrite();
    bool canWriteNR   = sender->rxCharacteristic->canWriteNoResponse();

    Serial.print("RX canWrite=");
    Serial.print(canWrite);
    Serial.print(" canWriteNoResp=");
    Serial.println(canWriteNR);

    String command = "{\"command\":\"set_score\",\"score\":" + String(newScore) + "}";
    Serial.print("📤 Syncing score to ");
    Serial.print(senderName.c_str());
    Serial.print(": ");
    Serial.println(command);

    bool ok = false;

    // Prefer no-response (safer / less blocking)
    if (canWriteNR) {
        ok = sender->rxCharacteristic->writeValue((uint8_t*)command.c_str(), command.length(), false);
        sendScoreNew();
    } else if (canWrite) {
        // fallback with response (can still block if remote misbehaves)
        ok = sender->rxCharacteristic->writeValue((uint8_t*)command.c_str(), command.length(), true);
    } else {
        Serial.println("❌ RX characteristic is not writable");
        return false;
    }

    Serial.println(ok ? "✅ Score sync sent" : "❌ Failed to send score sync");
    return ok;
}

// Callback for commands from PWA
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String raw = pCharacteristic->getValue();
    String msg = String(raw.c_str());
    msg.trim();

    if (msg.length() == 0) return;

    Serial.print("Command from PWA: ");
    Serial.println(msg);

    // ---------- JSON COMMANDS ----------
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

      // ---- CHANGE NAMES ----
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

        changeNames(newHome, newAway);  //  CALL
        return;
      } else if (cmd == "TIME") {
        Serial.println("Change time command received");
        return;
      } else if (cmd == "SCORE") {
        JsonArray values = doc["Value"];

        if (!values || values.size() < 2) {
            Serial.println("SCORE requires Value[2]");
            return;
        }

        String teamStr = values[0].as<String>();
        String opStr   = values[1].as<String>();
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

        // Clamp score (0–99)  ✅ constrain on the VALUE
        *scorePtr = constrain(*scorePtr, 0, 99);

        Serial.print("SCORE updated → ");
        Serial.print(teamStr);
        Serial.print(" = ");
        Serial.println(*scorePtr);

        // ---------- STUUR SCORE NAAR SENDER ----------
        std::string teamKey = teamStr.c_str();
        auto it = teamToSender.find(teamKey);
        if (it != teamToSender.end()) {
            const std::string& senderName = it->second;

            Serial.print("🔁 Syncing score to sender: ");
            Serial.println(senderName.c_str());

            // pass INT value ✅
            gSync.senderName = senderName;
            gSync.score = *scorePtr;
            gSync.pending = true;
            gSyncPending = true;

            Serial.println("Queued score sync (will send in loop)");
        } else {
            Serial.print("❌ No sender mapping found for team: ");
            Serial.println(teamStr);
        }

        return;
        }
    }
    // ---------- LEGACY STRING COMMANDS ----------
    if (msg == "get_devices") {
      sendDeviceListToPWA();
    } 
    else if (msg == "reset") {
      Serial.println("Reset command received");
    } 
    else if (msg == "hello") {
      timerReset();
    }
  }
};

// Send device list to PWA
void sendDeviceListToPWA(){
    if (!pwaConnected || !pDataCharacteristic) {
        Serial.println("Cannot send device list: PWA not connected");
        return;
    }

    String deviceList = "[";
    bool first = true;

    for (auto& devicePair : senderDevices) {
        if (!first)
            deviceList += ",";
        first = false;

        deviceList += "{";
        deviceList += "\"name\":\"" + String(devicePair.first.c_str()) + "\",";
        deviceList += "\"connected\":" + String(devicePair.second->connected ? "true" : "false") + ",";
        deviceList += "\"rssi\":" + String(devicePair.second->rssi) + ",";
        deviceList += "\"data\":\"" + devicePair.second->lastData + "\"";
        deviceList += "}";

        Serial.print("Device ");
        Serial.print(devicePair.first.c_str());
        Serial.print(" data: ");
        Serial.println(devicePair.second->lastData);
    }

    deviceList += "]";

    pDataCharacteristic->setValue(deviceList.c_str());
    pDataCharacteristic->notify();

    Serial.print("Sent device list to PWA: ");
    Serial.println(deviceList);
}

// Send sensor data to PWA
void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi){
    if (!pwaConnected || !pDataCharacteristic) {
        Serial.println("Cannot send data: PWA not connected");
        return;
    }

    // Format: {"sender":"DeviceName","data":"ActualDataFromSender","rssi":-50,"timestamp":123456}
    String jsonData = "{";
    jsonData += "\"sender\":\"" + String(senderName.c_str()) + "\",";
    jsonData += "\"data\":\"" + data + "\","; // ACTUAL data from sender
    jsonData += "\"rssi\":" + String(rssi) + ",";
    jsonData += "\"timestamp\":" + String(millis());
    jsonData += "}";

    pDataCharacteristic->setValue(jsonData.c_str());
    pDataCharacteristic->notify();

    Serial.print("📤 To PWA: ");
    Serial.println(jsonData);
}

// Check if device is a target sender
bool isTargetSender(const std::string& deviceName){
    for (const auto& target : targetSenders) {
        if (deviceName == target) {
            return true;
        }
    }
    return false;
}

// Helper function to extract score from possibly broken JSON
String extractScoreFromData(String data){
    // Try to find "score": pattern
    int scorePos = data.indexOf("\"score\":");

    if (scorePos != -1) {
        // Found JSON with score field
        int colonPos = scorePos + 8; // Length of "\"score\":"

        // Find the end of the score number
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

    // If no JSON found, try to parse as plain number
    data.trim();
    for (int i = 0; i < data.length(); i++) {
        if (!isdigit(data.charAt(i))) {
            // Not a pure number, try to extract digits
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

    // If we get here, it might be a plain number
    return data;
}

static void senderNotificationCallback(BLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    // Find which sender this belongs to
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

    // Get RSSI for this sender
    int rssi = -99;
    if (senderDevices.find(senderName) != senderDevices.end()) {
        rssi = senderDevices[senderName]->rssi;
    }

    // Convert data to string
    String receivedData = "";
    for (int i = 0; i < length; i++) {
        receivedData += (char)pData[i];
    }

    Serial.print("From ");
    Serial.print(senderName.c_str());
    Serial.print(": ");
    Serial.println(receivedData.c_str());

    // Extract score from the data (handle broken JSON)
    String scoreStr = extractScoreFromData(receivedData);

    // Update device info with extracted score
    if (senderDevices.find(senderName) != senderDevices.end()) {
        senderDevices[senderName]->lastData = scoreStr;
        senderDevices[senderName]->lastUpdate = millis();

        Serial.print("Updated ");
        Serial.print(senderName.c_str());
        Serial.print(" score to: ");
        Serial.println(scoreStr);
    }

    int sender = 0; // Unknown team
    if (senderName == "ESP32-BLE-Sender-1") {
        sender = 1; // Home team
    } else if (senderName == "ESP32-BLE-Sender-2") {
        sender = 2; // Away team
    }

    // Forward just the score to PWA
    sendSensorDataToPWA(senderName, scoreStr, rssi);
    scoreChange(scoreStr.toInt(), sender); // 0 = unknown team
}

// Connect to a sender device
void connectToSender(BLEAdvertisedDevice* device)
{
    std::string deviceName = device->getName().c_str();

    Serial.print("Connecting to sender: ");
    Serial.println(deviceName.c_str());

    // Check if already connected
    if (senderDevices.find(deviceName) != senderDevices.end() && 
        senderDevices[deviceName]->connected) {
        Serial.println("Already connected, skipping");
        return;
    }

    // Create client for this sender
    BLEClient* pClient = BLEDevice::createClient();
    
    // Set a timeout for connection
    pClient->setClientCallbacks(nullptr);

    Serial.print("   Attempting connection to: ");
    Serial.println(device->getAddress().toString().c_str());

    // Try to connect with timeout
    bool connected = false;
    unsigned long connectStart = millis();
    
    while (millis() - connectStart < 5000) { // 5 second timeout
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

    // Wait a bit for services to be discovered
    delay(100);

    // Get the service
    BLERemoteService* pRemoteService = pClient->getService(SENDER_SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found");
        pClient->disconnect();
        delete pClient;
        return;
    }

    Serial.println("Service found");

    // Get the TX characteristic
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
    // Store device info
    SenderDevice* sender = new SenderDevice();
    sender->name = deviceName;
    sender->address = device->getAddress();
    sender->client = pClient;
    sender->txCharacteristic = pRemoteTX;
    sender->rxCharacteristic = pRemoteRX;
    sender->connected = true;
    sender->rssi = device->getRSSI();
    sender->lastUpdate = millis();
    sender->lastData = "0"; // Default score

    senderDevices[deviceName] = sender;
    characteristicToSenderMap[pRemoteTX] = deviceName;

    // Register for notifications
    if (pRemoteTX->canNotify()) {
        Serial.println("Registering for notifications...");
        
        // Register the callback FIRST
        pRemoteTX->registerForNotify(senderNotificationCallback);
        
        // Then enable notifications
        uint8_t cccdValue[] = {0x01, 0x00};
        BLERemoteDescriptor* pCCCD = pRemoteTX->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pCCCD != nullptr) {
            if (pCCCD->writeValue(cccdValue, 2, true)) {
                Serial.println("Notifications enabled");
            } else {
                Serial.println("Failed to enable notifications");
            }
        }
    }

    // Try to read initial value
    std::string value = pRemoteTX->readValue().c_str();
    if (!value.empty()) {
        sender->lastData = String(value.c_str());
        Serial.print("Initial value: ");
        Serial.println(value.c_str());
    }

    Serial.print("Successfully connected to sender: ");
    Serial.println(deviceName.c_str());

    // Notify PWA if connected
    if (pwaConnected) {
        sendDeviceListToPWA();
    }
}
// Scan for sender devices
void scanForSenders()
{
    Serial.println("\nScanning for sender devices...");
    Serial.println("Temporarily pausing PWA advertising for scan...");

    // Pause PWA advertising during scan
    BLEDevice::stopAdvertising();

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    // Scan for 3 seconds
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

                // Check if already connected
                if (senderDevices.find(deviceName) != senderDevices.end() && 
                    senderDevices[deviceName]->connected) {
                    Serial.println("Already connected");
                    senderDevices[deviceName]->rssi = device.getRSSI();
                    senderDevices[deviceName]->lastUpdate = millis();
                    connected++;
                } else {
                    Serial.println("Attempting connection...");
                    connectToSender(&device);
                    if (senderDevices.find(deviceName) != senderDevices.end() && 
                        senderDevices[deviceName]->connected) {
                        connected++;
                    }
                }
            }
        }
    }

    pBLEScan->clearResults();

    // Restart PWA advertising
    BLEDevice::startAdvertising();
    Serial.println("Restarted PWA advertising");

    Serial.print("\nScan Summary: ");
    Serial.print(targetFound);
    Serial.print(" target(s) found, ");
    Serial.print(connected);
    Serial.println(" connected");
}

// Setup BLE Gateway
void setupBLEGateway()
{
    Serial.println("Setting up BLE Gateway...");

    // Initialize BLE
    BLEDevice::init(DEVICE_NAME);
    
    // Set power to maximum
    esp_power_level_t power = ESP_PWR_LVL_P9;
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, power);

    // Create BLE Server for PWA
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new GatewayServerCallbacks());

    // Create Service for PWA
    BLEService* pService = pServer->createService(GATEWAY_SERVICE_UUID);

    // Create Data Characteristic
    pDataCharacteristic = pService->createCharacteristic(
        GATEWAY_CHAR_DATA_UUID, 
        BLECharacteristic::PROPERTY_NOTIFY | 
        BLECharacteristic::PROPERTY_READ
    );
    pDataCharacteristic->addDescriptor(new BLE2902());

    // Create Status Characteristic
    pStatusCharacteristic = pService->createCharacteristic(
        GATEWAY_CHAR_STATUS_UUID, 
        BLECharacteristic::PROPERTY_NOTIFY | 
        BLECharacteristic::PROPERTY_READ
    );
    pStatusCharacteristic->addDescriptor(new BLE2902());

    // Create Command Characteristic
    pCommandCharacteristic = pService->createCharacteristic(
        GATEWAY_CHAR_COMMAND_UUID, 
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    // Start the service
    pService->start();

    // Configure advertising for PWA
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    
    // IMPORTANT: Add service UUID (this is what PWA scans for)
    pAdvertising->addServiceUUID(GATEWAY_SERVICE_UUID);
    
    // Set scan response
    pAdvertising->setScanResponse(true);
    
    // CRITICAL FOR PWA: Use standard advertising parameters
    // Most PWAs expect these standard values
    pAdvertising->setMinInterval(0x20);    // 25ms
    pAdvertising->setMaxInterval(0x40);    // 50ms
    
    // Standard preferred values that work with most BLE scanners
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    // Start advertising
    BLEDevice::startAdvertising();

    Serial.println("\nBLE Gateway READY for PWA");
    Serial.println("Advertising with:");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
    Serial.print("Service UUID: ");
    Serial.println(GATEWAY_SERVICE_UUID);
    Serial.println("Advertising interval: 25-50ms");
    Serial.println("Scan response: Enabled");
    Serial.println("\nPWA should find this device!");
}

void bleInit(){
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

    // Setup BLE Gateway for PWA
    setupBLEGateway();

    // Initial scan for senders
    scanForSenders();
}

void handleBluetooth(){
    // Handle PWA connection state
    if (!pwaConnected && oldPwaConnected) {
        // delay(500);
        oldPwaConnected = pwaConnected;
    }

    if (pwaConnected && !oldPwaConnected) {
        oldPwaConnected = pwaConnected;
        // Send current device list when PWA connects
        sendDeviceListToPWA();
    }

    // Periodic scan for senders (every 30 seconds)
    static unsigned long lastScanTime = 0;
    if (millis() - lastScanTime >= 30000) {
        scanForSenders();
        lastScanTime = millis();
    }
    if (gSyncPending && gSync.pending) {
        gSyncPending = false;     // eerst flag omlaag (voorkomt spam)
        gSync.pending = false;

        Serial.print("🚀 Sending queued score sync to ");
        Serial.print(gSync.senderName.c_str());
        Serial.print(" score=");
        Serial.println(gSync.score);

        syncScoreToSender(gSync.senderName, gSync.score);
    }

    // delay(100);
}