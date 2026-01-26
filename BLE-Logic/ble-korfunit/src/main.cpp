#include "korfunit.h"
#include "ble_handler.h"
#include "goal_detector.h"

void setup() {
    setupKorfUnit();
}

void loop() {
    loopKorfUnit();
}

void setupKorfUnit() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n══════════════════════════════════════");
    Serial.println("ESP32-C3 KorfUnit ");
    Serial.println("══════════════════════════════════════");
    
    // Initialize BLE
    setupBLE();
    
    // Initialize sensors
    setupSensors();
    
    Serial.println("\n✅ System Ready!");
    Serial.println("Waiting for goals and commands...");
    Serial.println("══════════════════════════════════════\n");
}

void loopKorfUnit() {
    // Check for goals (sensor detection)
    checkForGoal();
    
    // Handle BLE communication and connection health
    loopBLE();
    
    delay(10);
}