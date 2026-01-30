#include "korfunit.h"
#include "ble_handler.h"
#include "goal_detector.h"
#include "ImpactDetection.h"
#include <Wire.h>

ImpactDetection impact;
static unsigned long lastScoreSend = 0;


void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting KorfUnit System...");
    
    // Initialize I2C once for MPU6050
    // Use the pins defined in korfunit.h
    Wire.setPins(MPU_SDA, MPU_SCL);
    Wire.begin();
    Wire.setClock(400000);  // MPU6050 works well at 400kHz
    Wire.setTimeOut(50);    // Reasonable timeout
    
    Serial.println("I2C initialized for MPU6050");
    
    // Setup BLE
    setupBLE();
    
    // Initialize MPU6050
    // impact.begin();
    delay(500);
    
    // Initialize ToF sensors
    // Note: ToF sensors will need their own I2C bus or reset
    setupSensors();
    delay(500);
    
    Serial.println("=== System Ready ===");
}

void loop() {
    // Check for basketball goals
    checkForGoal();
    
    // Handle BLE communication
    loopBLE();
    
    // Update MPU and run Edge Impulse inference
    //impact.update();
    
    // Small delay to prevent watchdog
    delay(10);
}