#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <website.h>
#include <ArduinoJson.h>

// Structure for connected sender devices
struct SenderDevice {
    std::string name;
    BLEAddress address;
    BLEClient* client;
    BLERemoteCharacteristic* txCharacteristic;
    bool connected;
    int rssi;
    String lastData;
    unsigned long lastUpdate;

    SenderDevice()
        : address(BLEAddress("00:00:00:00:00:00"))
        , client(nullptr)
        , txCharacteristic(nullptr)
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
        Serial.println("📱 PWA Connected!");

        // Update status characteristic
        String status = "connected";
        pStatusCharacteristic->setValue(status.c_str());
        pStatusCharacteristic->notify();
    }

    void onDisconnect(BLEServer* pServer)
    {
        pwaConnected = false;
        Serial.println("📱 PWA Disconnected!");

        // Update status characteristic
        String status = "disconnected";
        pStatusCharacteristic->setValue(status.c_str());
        pStatusCharacteristic->notify();

        // Restart advertising
        delay(500);
        pServer->startAdvertising();
        Serial.println("🔄 Advertising restarted for PWA");
    }
};

// Callback for commands from PWA
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    std::string raw = pCharacteristic->getValue();
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
        Serial.println("Change score command received");
        return;
      }

      Serial.println("Unknown JSON command");
      return;
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
        Serial.println("❌ Cannot send device list: PWA not connected");
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

        Serial.print("📊 Device ");
        Serial.print(devicePair.first.c_str());
        Serial.print(" data: ");
        Serial.println(devicePair.second->lastData);
    }

    deviceList += "]";

    pDataCharacteristic->setValue(deviceList.c_str());
    pDataCharacteristic->notify();

    Serial.print("📤 Sent device list to PWA: ");
    Serial.println(deviceList);
}

// Send sensor data to PWA
void sendSensorDataToPWA(const std::string& senderName, const String& data, int rssi){
    if (!pwaConnected || !pDataCharacteristic) {
        Serial.println("❌ Cannot send data: PWA not connected");
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

    // Forward just the score to PWA
    sendSensorDataToPWA(senderName, scoreStr, rssi);
}

// Connect to a sender device
void connectToSender(BLEAdvertisedDevice* device){
    std::string deviceName = std::string(device->getName().c_str());

    Serial.print("🔗 Connecting to sender: ");
    Serial.println(deviceName.c_str());

    // Create client for this sender
    BLEClient* pClient = BLEDevice::createClient();

    if (pClient->connect(device)) {
        Serial.println("   Connected to device");

        BLERemoteService* pRemoteService = pClient->getService(SENDER_SERVICE_UUID);
        if (pRemoteService == nullptr) {
            Serial.println("   ❌ Service not found");
            delete pClient;
            return;
        }

        Serial.println("   Service found");

        BLERemoteCharacteristic* pRemoteTX = pRemoteService->getCharacteristic(SENDER_CHAR_UUID_TX);
        if (pRemoteTX == nullptr) {
            Serial.println("   ❌ TX characteristic not found");
            delete pClient;
            return;
        }

        Serial.println("   TX characteristic found");

        // Store device info FIRST
        SenderDevice* sender = new SenderDevice();
        sender->name = deviceName;
        sender->address = device->getAddress();
        sender->client = pClient;
        sender->txCharacteristic = pRemoteTX;
        sender->connected = true;
        sender->rssi = device->getRSSI();
        sender->lastUpdate = millis();

        senderDevices[deviceName] = sender;
        characteristicToSenderMap[pRemoteTX] = deviceName;

        // Register for notifications
        if (pRemoteTX->canNotify()) {
            Serial.println("   Registering for notifications...");

            // Correct way to register for notifications
            pRemoteTX->registerForNotify(senderNotificationCallback);

            Serial.println("   ✅ Notification registration successful");
        }

        // Read initial value
        std::string value = std::string(pRemoteTX->readValue().c_str());
        if (!value.empty()) {
            sender->lastData = String(value.c_str());
            Serial.print("   Initial value: ");
            Serial.println(value.c_str());
        }

        Serial.print("✅ Connected to sender: ");
        Serial.println(deviceName.c_str());

        // Notify PWA about new connection
        if (pwaConnected) {
            sendDeviceListToPWA();
        }

        return;
    }

    // Connection failed
    Serial.print("❌ Failed to connect to sender: ");
    Serial.println(deviceName.c_str());
    delete pClient;
}

// Scan for sender devices
void scanForSenders(){
    Serial.println("\nScanning for sender devices...");

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    BLEScanResults foundDevices = pBLEScan->start(3, false); // 3 second scan


    Serial.print("Found ");
    Serial.print(foundDevices.getCount());
    Serial.println(" total devices");

    int targetFound = 0;

    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);

        if (device.haveName()) {
            std::string deviceName = std::string(device.getName().c_str());

            if (isTargetSender(deviceName)) {
                targetFound++;

                Serial.print("   Target found: ");
                Serial.print(deviceName.c_str());
                Serial.print(" (RSSI: ");
                Serial.print(device.getRSSI());
                Serial.println(" dBm)");

                // Check if already connected
                if (senderDevices.find(deviceName) == senderDevices.end() || !senderDevices[deviceName]->connected) {
                    // Connect to this sender
                    connectToSender(&device);
                } else {
                    // Update RSSI for connected device
                    senderDevices[deviceName]->rssi = device.getRSSI();
                    senderDevices[deviceName]->lastUpdate = millis();
                }
            }
        }
    }

    pBLEScan->clearResults();

    Serial.print("Found ");
    Serial.print(targetFound);
    Serial.println(" target sender(s)");
}

// Setup BLE Gateway
void setupBLEGateway(){
    Serial.println("Setting up BLE Gateway...");

    // Initialize BLE
    BLEDevice::init(DEVICE_NAME);

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);   // advertising
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);  // scanning (optional)
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9); // connection handle 0
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL1, ESP_PWR_LVL_P9); // (optional) if multiple conns
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL2, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL3, ESP_PWR_LVL_P9);

    // Create BLE Server for PWA
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new GatewayServerCallbacks());

    // Create Service for PWA
    BLEService* pService = pServer->createService(GATEWAY_SERVICE_UUID);

    // Create Data Characteristic
    pDataCharacteristic = pService->createCharacteristic(
        GATEWAY_CHAR_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    pDataCharacteristic->addDescriptor(new BLE2902());

    // Create Status Characteristic
    pStatusCharacteristic = pService->createCharacteristic(
        GATEWAY_CHAR_STATUS_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    pStatusCharacteristic->addDescriptor(new BLE2902());

    // Create Command Characteristic
    pCommandCharacteristic
        = pService->createCharacteristic(GATEWAY_CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(GATEWAY_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);

    BLEDevice::startAdvertising();

    Serial.println("BLE Gateway advertising for PWA");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
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

    // delay(100);
}