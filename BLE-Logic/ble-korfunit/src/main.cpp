#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_VL53L1X.h>

// Device name
#define DEVICE_NAME "ESP32-BLE-Sender-1"
// #define DEVICE_NAME "ESP32-BLE-Sender-2"

// Service UUID - Must match receiver
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// -------------------------
// Sensor Configuration - KEEP AS YOUR ORIGINAL WORKING CODE
// -------------------------
#define SDA_PIN 6
#define SCL_PIN 7
#define XSHUT_TOP 2      // GPIO2
#define XSHUT_BOTTOM 3   // GPIO3

Adafruit_VL53L1X tof_top(XSHUT_TOP);
Adafruit_VL53L1X tof_bottom(XSHUT_BOTTOM);

const int TRIGGER_DISTANCE = 300;
bool topTriggered = false;
int score = 0;
unsigned long lastGoalTime = 0;
const unsigned long GOAL_COOLDOWN = 1000;

// BLE objects
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t connectionCount = 0;

// -------------------------
// BLE Callbacks - USING WORKING BLE CODE STRUCTURE
// -------------------------
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        connectionCount++;
        Serial.println("✅ Device connected!");
        Serial.print("Connection count: ");
        Serial.println(connectionCount);
        
        // Send current score on connection (IN JSON FORMAT)
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
        std::string rxValue = pCharacteristic->getValue().c_str();
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
                
                // Send updated score IN JSON FORMAT
                String message = "{\"score\":" + String(score) + ",\"type\":\"update\"}";
                pTxCharacteristic->setValue(message.c_str());
                pTxCharacteristic->notify();
                Serial.print("📤 Sent reset score: ");
                Serial.println(message);
                
                // Reset sensor state
                topTriggered = false;
            }
        }
    }
};

// -------------------------
// Sensor Initialization - USING YOUR ORIGINAL WORKING METHOD
// -------------------------
void initSensors() {
    Serial.println("Initializing VL53L1X sensors...");
    
    // Setup I2C
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    
    // Setup XSHUT pins
    pinMode(XSHUT_TOP, OUTPUT);
    pinMode(XSHUT_BOTTOM, OUTPUT);
    digitalWrite(XSHUT_TOP, LOW);
    digitalWrite(XSHUT_BOTTOM, LOW);
    delay(10);
    
    // Initialize TOP sensor with address 0x30
    digitalWrite(XSHUT_TOP, HIGH);
    delay(10);
    
    if (!tof_top.begin(0x30, &Wire)) {
        Serial.println("❌ Top sensor init failed!");
        // Try default address as fallback
        if (!tof_top.begin(0x29, &Wire)) {
            Serial.println("❌ Top sensor completely failed!");
        } else {
            Serial.println("⚠️ Top sensor using default address 0x29");
        }
    } else {
        Serial.println("✅ Top sensor found at 0x30");
    }
    
    // Initialize BOTTOM sensor with address 0x31
    digitalWrite(XSHUT_BOTTOM, HIGH);
    delay(10);
    
    if (!tof_bottom.begin(0x31, &Wire)) {
        Serial.println("❌ Bottom sensor init failed!");
        // Try alternative addresses
        if (!tof_bottom.begin(0x32, &Wire)) {
            if (!tof_bottom.begin(0x29, &Wire)) {
                Serial.println("❌ Bottom sensor completely failed!");
            } else {
                Serial.println("⚠️ Bottom sensor using default address 0x29");
            }
        } else {
            Serial.println("⚠️ Bottom sensor using address 0x32");
        }
    } else {
        Serial.println("✅ Bottom sensor found at 0x31");
    }
    
    // Configure sensors
    tof_top.setTimingBudget(50);
    tof_bottom.setTimingBudget(50);
    
    // Start ranging
    tof_top.startRanging();
    tof_bottom.startRanging();
    
    Serial.println("✅ Sensors initialized!");
}

// -------------------------
// Sensor Reading
// -------------------------
bool readTopSensor() {
    if (tof_top.dataReady()) {
        int distance = tof_top.distance();
        tof_top.clearInterrupt();
        
        if (distance > 0 && distance < 4000) {
            // Optional: Print distance for debugging
            // Serial.print("Top distance: ");
            // Serial.print(distance);
            // Serial.println(" mm");
            
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}

bool readBottomSensor() {
    if (tof_bottom.dataReady()) {
        int distance = tof_bottom.distance();
        tof_bottom.clearInterrupt();
        
        if (distance > 0 && distance < 4000) {
            // Optional: Print distance for debugging
            // Serial.print("Bottom distance: ");
            // Serial.print(distance);
            // Serial.println(" mm");
            
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}

// -------------------------
// Send score via BLE
// -------------------------
void sendScoreToBLE(const char* type = "update") {
    if (deviceConnected && pTxCharacteristic != nullptr) {
        String message = "{\"score\":" + String(score) + ",\"type\":\"" + String(type) + "\"}";
        pTxCharacteristic->setValue(message.c_str());
        pTxCharacteristic->notify();
        
        Serial.print("📤 Sent to gateway: ");
        Serial.println(message);
    } else {
        Serial.print("❌ Not connected to gateway. Current score: ");
        Serial.println(score);
    }
}

// -------------------------
// Goal Detection
// -------------------------
void checkForGoal() {
    unsigned long currentTime = millis();
    
    // Cooldown check
    if (currentTime - lastGoalTime < GOAL_COOLDOWN) {
        return;
    }
    
    bool topActive = readTopSensor();
    bool bottomActive = readBottomSensor();
    
    // Debug sensor states
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime > 1000) {
        lastDebugTime = currentTime;
        if (tof_top.dataReady() && tof_bottom.dataReady()) {
            Serial.print("📊 Sensors active - Top triggered: ");
            Serial.print(topTriggered ? "YES" : "NO");
            Serial.print(", Top active: ");
            Serial.print(topActive ? "YES" : "NO");
            Serial.print(", Bottom active: ");
            Serial.println(bottomActive ? "YES" : "NO");
        }
    }
    
    if (topActive && !topTriggered) {
        topTriggered = true;
        Serial.println("📊 Top sensor triggered - waiting for bottom...");
    }
    
    if (topTriggered && bottomActive) {
        // GOAL!
        score++;
        lastGoalTime = currentTime;
        topTriggered = false;
        
        Serial.print("🎯 GOAL DETECTED! New score: ");
        Serial.println(score);
        
        // Send via BLE IN JSON FORMAT
        sendScoreToBLE("goal");
        

        
        // Brief delay to prevent double counting
        delay(100);
    }
    
    // // Timeout reset (if top was triggered but bottom never came)
    // if (topTriggered && (currentTime - lastGoalTime > 3000)) {
    //     topTriggered = false;
    //     Serial.println("⏰ Top sensor timeout - resetting");
    // }
}

// -------------------------
// Setup
// -------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n══════════════════════════════════════");
    Serial.println("ESP32-C3 KorfUnit");
    Serial.println("══════════════════════════════════════");
    

    
    // STEP 1: Initialize BLE FIRST (using working BLE setup)
    Serial.println("📡 Initializing BLE...");
    BLEDevice::init(DEVICE_NAME);
    
    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Create Service
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Create TX Characteristic (with BLE2902 descriptor)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY | 
        BLECharacteristic::PROPERTY_READ
    );
    
    // CRITICAL: Add this descriptor for notifications to work
    pTxCharacteristic->addDescriptor(new BLE2902());
    
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
    
    Serial.println("✅ BLE Advertising started!");
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
    Serial.print("Service UUID: ");
    Serial.println(SERVICE_UUID);
    
    // STEP 2: Initialize sensors
    initSensors();
    
    Serial.println("\n✅ System Ready!");
    Serial.println("Waiting for goals...");
    Serial.println("══════════════════════════════════════\n");
}

// -------------------------
// Main Loop
// -------------------------
void loop() {
    // Check for goals
    checkForGoal();
    
    // Handle BLE connection state
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        oldDeviceConnected = deviceConnected;
        Serial.println("🔄 Restarted advertising");
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("✅ Device connected!");
    }
    
    // Small delay to prevent watchdog
    delay(10);
}