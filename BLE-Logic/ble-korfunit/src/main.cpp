#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <time.h>

// Device name - MAKE EACH SENDER UNIQUE!
// For Sender 1 (Home team):
#define DEVICE_NAME "ESP32-BLE-Sender-1"
// For Sender 2 (Away team):
// #define DEVICE_NAME "ESP32-BLE-Sender-2"

// Service UUID - Must match receiver
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// Score tracking
int score = 0;
unsigned long lastScoreTime = 0;
unsigned long nextScoreInterval = 0;

// Random interval settings (in milliseconds)
const unsigned long MIN_INTERVAL = 3000;  // 3 seconds
const unsigned long MAX_INTERVAL = 10000; // 10 seconds

// BLE objects
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t connectionCount = 0;

// Function to generate random interval
unsigned long getRandomInterval() {
  return random(MIN_INTERVAL, MAX_INTERVAL);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        connectionCount++;
        Serial.println("✅ Device connected!");
        Serial.print("Connection count: ");
        Serial.println(connectionCount);
        
        // Send current score on connection
        if (pTxCharacteristic != nullptr) {
            String message = "{\"score\":" + String(score) + ",\"type\":\"init\"}";
            pTxCharacteristic->setValue(message.c_str());
            pTxCharacteristic->notify();
            Serial.print("📤 Sent initial score: ");
            Serial.println(message);
        }
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("❌ Device disconnected!");
        delay(500);
        pServer->startAdvertising();
        Serial.println("🔄 Restarted advertising");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            Serial.print("📥 Received from app: ");
            for (int i = 0; i < rxValue.length(); i++) {
                Serial.print(rxValue[i]);
            }
            Serial.println();
            
            // Handle commands from app
            if (rxValue == "reset") {
                score = 0;
                Serial.println("🔁 Score reset to 0");
                
                // Send updated score
                String message = "{\"score\":" + String(score) + ",\"type\":\"update\"}";
                pTxCharacteristic->setValue(message.c_str());
                pTxCharacteristic->notify();
                Serial.print("📤 Sent reset score: ");
                Serial.println(message);
                
                // Reset timer
                nextScoreInterval = getRandomInterval();
                lastScoreTime = millis();
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize random seed with noise from analog pin
    randomSeed(analogRead(0) + micros());
    
    Serial.println("\n=================================");
    Serial.println("ESP32C3 BLE Score Sender");
    Serial.println("=================================");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
    Serial.println("Auto-incrementing scores on random timer");
    Serial.print("Interval: ");
    Serial.print(MIN_INTERVAL / 1000);
    Serial.print(" - ");
    Serial.print(MAX_INTERVAL / 1000);
    Serial.println(" seconds");
    Serial.println("=================================\n");
    
    // Initialize BLE
    BLEDevice::init(DEVICE_NAME);
    
    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Create Service
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Create TX Characteristic
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY | 
        BLECharacteristic::PROPERTY_READ
    );
    
    // Create RX Characteristic
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());
    
    // Start service
    pService->start();
    
    // Start advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    Serial.println("✅ BLE Advertising started");
    
    // Initialize first random interval
    nextScoreInterval = getRandomInterval();
    lastScoreTime = millis();
    Serial.print("First score in: ");
    Serial.print(nextScoreInterval / 1000);
    Serial.println(" seconds");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Check if it's time to increment score
    if (currentTime - lastScoreTime >= nextScoreInterval) {
        score++;
        
        Serial.print("🎯 Auto-score incremented: ");
        Serial.print(score);
        Serial.print(" (after ");
        Serial.print(nextScoreInterval / 1000);
        Serial.println(" seconds)");
        
        // Send score update via BLE if connected
        if (deviceConnected && pTxCharacteristic != nullptr) {
            // Create JSON message
            String message = "{\"score\":" + String(score) + ",\"type\":\"update\"}";
            
            pTxCharacteristic->setValue(message.c_str());
            pTxCharacteristic->notify();
            
            Serial.print("📤 Sent to app: ");
            Serial.println(message);
        }
        
        
        
        // Set next random interval
        nextScoreInterval = getRandomInterval();
        lastScoreTime = currentTime;
        
        Serial.print("Next score in: ");
        Serial.print(nextScoreInterval / 1000);
        Serial.println(" seconds");
    }
    
    // Handle connection state
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    
    // Small delay to prevent watchdog
    delay(10);
}