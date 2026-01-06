#include <Arduino.h>
#include "SensorManager.h"

SensorManager sensorManager;

// Configuratie
const unsigned long UPDATE_INTERVAL_MS = 50;  // 20Hz update rate
const unsigned long IDLE_TIMEOUT_MS = 300000; // 5 minuten inactief = deep sleep
const unsigned long SLEEP_TIME_SECONDS = 600; // 10 minuten deep sleep

unsigned long lastUpdateTime = 0;
unsigned long lastActivityTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=================================");
    Serial.println("Korfbal Detection System v1.0");
    Serial.println("ESP32-C3 XIAO");
    Serial.println("=================================\n");
    
    // Initialiseer sensor systeem
    if (!sensorManager.begin()) {
        Serial.println("❌ System initialization failed!");
        Serial.println("Please check sensor connections and restart.");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("✅ System ready!");
    Serial.println("Waiting for ball detection...\n");
    
    lastActivityTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    
    // Update sensoren met vaste interval
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        lastUpdateTime = currentTime;
        
        sensorManager.update();
        
        // Check voor events
        if (sensorManager.isGoalScored()) {
            Serial.println("\n🎉🎉🎉 DOELPUNT!  🎉🎉🎉\n");
            lastActivityTime = currentTime;
            
            // Hier kun je extra acties toevoegen (LED, buzzer, WiFi transmissie, etc.)
            
            sensorManager.resetDetections();
        }
        
        if (sensorManager.isShotAttempt()) {
            Serial.println("📊 Shot attempt registered");
            lastActivityTime = currentTime;
            
            // Log shot attempt
            
            delay(1000); // Debounce
            sensorManager.resetDetections();
        }
        
        if (sensorManager.isPlayerCollision()) {
            Serial.println("⚠️  Possible foul - player collision detected");
            lastActivityTime = currentTime;
            
            delay(1000); // Debounce
            sensorManager.resetDetections();
        }
    }
    
    // Energie besparing: deep sleep na inactiviteit
    if (currentTime - lastActivityTime > IDLE_TIMEOUT_MS) {
        Serial.println("\n💤 System idle - entering deep sleep mode");
        sensorManager.printStatus();
        sensorManager.enterDeepSleep(SLEEP_TIME_SECONDS);
    }
    
    // Kleine delay om WDT te voeden
    delay(1);
}