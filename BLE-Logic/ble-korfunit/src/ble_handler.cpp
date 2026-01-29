#include "ble_handler.h"
#include "korfunit.h"

// BLE objects
static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static BLECharacteristic* pRxCharacteristic = nullptr;

// BLE state
bool deviceConnected = false;
static bool oldDeviceConnected = false;
bool connectionValid = false;
static uint32_t connectionCount = 0;
static unsigned long lastPingTime = 0;
static unsigned long connectionTimeout = 0;

// Score variable (shared with goal detector)
int score = 0;
String incomingBuffer = ""; // global or static
// GatewayCommandCallbacks implementation
void GatewayCommandCallbacks::onWrite(BLECharacteristic* pCharacteristic)
{
    String chunk = pCharacteristic->getValue();
    if (chunk.length() == 0)
        return;

    incomingBuffer += chunk; // append fragment

    Serial.print("📥 Received chunk: ");
    Serial.println(chunk);

    // Only attempt JSON parsing if it looks complete
    if (incomingBuffer.startsWith("{") && incomingBuffer.endsWith("}")) {
        DynamicJsonDocument doc(200);
        DeserializationError err = deserializeJson(doc, incomingBuffer);

        if (err) {
            Serial.print("❌ JSON parse error: ");
            Serial.println(err.c_str());
            incomingBuffer = ""; // reset buffer to avoid infinite retry
            return;
        }

        // process command
        if (doc["command"].is<const char*>()) {
            String command = doc["command"].as<String>();
            if (command == "set_score" && doc["score"].is<int>()) {
                score = doc["score"];
                Serial.print("🎯 Score set by gateway: ");
                Serial.println(score);
                sendScoreToBLE("sync");
            }
        }

        incomingBuffer = ""; // reset after processing
    }
}

// MyServerCallbacks implementation
void MyServerCallbacks::onConnect(BLEServer* pServer)
{
    deviceConnected = true;
    connectionValid = true;
    connectionCount++;

    // Get negotiated MTU for this connection
    uint16_t mtu = pServer->getPeerMTU(pServer->getConnId());
    Serial.print("✅ Connected, negotiated MTU: ");
    Serial.println(mtu);
    connectionTimeout = 0;
    lastPingTime = 0;

    Serial.println("✅ Gateway connected!");

    if (pTxCharacteristic != nullptr) {
        String message = "{\"score\":" + String(score) + ",\"type\":\"init\",\"status\":\"connected\"}";
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();
        Serial.print("📤 Sent initial score: ");
        Serial.println(message);
    }
}

void MyServerCallbacks::onDisconnect(BLEServer* pServer)
{
    deviceConnected = false;
    connectionValid = false;
    connectionTimeout = 0;
    lastPingTime = 0;

    Serial.println("❌ Gateway disconnected!");

    delay(500);

    // Clean restart of advertising
    BLEDevice::deinit(true);
    delay(100);
    BLEDevice::init(DEVICE_NAME);

    // Recreate everything
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);

    pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(new GatewayCommandCallbacks());

    pService->start();

    esp_power_level_t power = ESP_PWR_LVL_P9;
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, power);

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("🔄 Advertising restarted from scratch");
}

// BLE management functions
void setupBLE()
{
    Serial.println("📡 Initializing BLE...");
    BLEDevice::init(DEVICE_NAME);
    BLEDevice::setMTU(512);

    esp_power_level_t power = ESP_PWR_LVL_P9;
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, power);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);

    pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(new GatewayCommandCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("✅ BLE Advertising started!");
    Serial.println("🔄 Ready for bidirectional communication with ping");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
    Serial.println("📡 Ping interval: 3 seconds");
    Serial.println("⏱️ Connection timeout: 10 seconds");
}

void loopBLE()
{
    checkConnectionHealth();

    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        oldDeviceConnected = deviceConnected;
        Serial.println("🔄 Restarted advertising");
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("✅ Device connected!");
    }
}

void sendScoreToBLE(const char* type)
{
    if (deviceConnected && pTxCharacteristic != nullptr) {
        String message = "{\"score\":" + String(score) + ",\"type\":\"" + String(type) + "\"}";
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();

        Serial.print("📤 Sent to gateway: ");
        Serial.println(message);
        connectionValid = true;
    } else {
        Serial.print("❌ Not connected to gateway. Current score: ");
        Serial.println(score);
    }
}

void sendPing()
{
    if (deviceConnected && pTxCharacteristic != nullptr) {
        String pingMsg = "{\"status\":\"ping\",\"timestamp\":" + String(millis()) + "}";
        pTxCharacteristic->setValue(pingMsg.c_str());
        pTxCharacteristic->notify();

        connectionValid = true;
        connectionTimeout = 0;
        Serial.println("📡 Ping sent (connection good)");
    }
}

void checkConnectionHealth()
{
    if (deviceConnected) {
        if (millis() - lastPingTime > PING_INTERVAL) {
            sendPing();
            lastPingTime = millis();
        }

        if (!connectionValid) {
            if (connectionTimeout == 0) {
                connectionTimeout = millis();
                Serial.println("⚠️ Connection appears weak");
            }

            if (millis() - connectionTimeout > CONNECTION_TIMEOUT) {
                Serial.println("⚠️ Connection lost - forcing disconnect");
                deviceConnected = false;
                connectionValid = false;
                connectionTimeout = 0;
                lastPingTime = 0;
            }
        } else {
            connectionTimeout = 0;
        }
    }
}

// Accessor functions
bool isDeviceConnected() { return deviceConnected; }

bool isConnectionValid() { return connectionValid; }

void setScore(int newScore) { score = newScore; }