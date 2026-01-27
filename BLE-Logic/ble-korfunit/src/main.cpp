#include "korfunit.h"
#include "ble_handler.h"
#include "goal_detector.h"
#include "ImpactDetection.h"

ImpactDetection impact;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Longer delay for stability
    
    Serial.println("\n══════════════════════════════════════");
    Serial.println("ESP32-C3 KorfUnit - Modular Design");
    Serial.println("══════════════════════════════════════");
    
    // Initialize BLE
    setupBLE();
    
    // Initialize sensors (will work even if not connected)
    setupSensors();
    impact.begin();
    
    Serial.println("\n✅ System Ready!");
    Serial.println("══════════════════════════════════════\n");
}

void loop() {
    // Check for goals (sensor detection)
    checkForGoal();  // This will return immediately if sensors disabled
    
    // Handle BLE communication and connection health
    loopBLE();
    impact.update();
    
    delay(10);
}