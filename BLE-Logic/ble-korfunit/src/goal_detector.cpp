#include "goal_detector.h"
#include "ble_handler.h"

// Sensor objects
static Adafruit_VL53L1X tof_top(XSHUT_TOP);
static Adafruit_VL53L1X tof_bottom(XSHUT_BOTTOM);

// Goal detection state
static bool topTriggered = false;
static unsigned long lastGoalTime = 0;
static unsigned long lastDebugTime = 0;

void setupSensors() {
    Serial.println("Initializing VL53L1X sensors...");
    
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    
    pinMode(XSHUT_TOP, OUTPUT);
    pinMode(XSHUT_BOTTOM, OUTPUT);
    digitalWrite(XSHUT_TOP, LOW);
    digitalWrite(XSHUT_BOTTOM, LOW);
    delay(10);
    
    digitalWrite(XSHUT_TOP, HIGH);
    delay(10);
    
    if (!tof_top.begin(0x30, &Wire)) {
        if (!tof_top.begin(0x29, &Wire)) {
            Serial.println("❌ Top sensor completely failed!");
        } else {
            Serial.println("⚠️ Top sensor using default address 0x29");
        }
    } else {
        Serial.println("✅ Top sensor found at 0x30");
    }
    
    digitalWrite(XSHUT_BOTTOM, HIGH);
    delay(10);
    
    if (!tof_bottom.begin(0x31, &Wire)) {
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
    
    tof_top.setTimingBudget(50);
    tof_bottom.setTimingBudget(50);
    
    tof_top.startRanging();
    tof_bottom.startRanging();
    
    Serial.println("✅ Sensors initialized!");
}

static bool readTopSensor() {
    if (tof_top.dataReady()) {
        int distance = tof_top.distance();
        tof_top.clearInterrupt();
        
        if (distance > 0 && distance < 4000) {
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}

static bool readBottomSensor() {
    if (tof_bottom.dataReady()) {
        int distance = tof_bottom.distance();
        tof_bottom.clearInterrupt();
        
        if (distance > 0 && distance < 4000) {
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}

void checkForGoal() {
    unsigned long currentTime = millis();
    
    // Check goal cooldown
    if (currentTime - lastGoalTime < GOAL_COOLDOWN) {
        return;
    }
    
    bool topActive = readTopSensor();
    bool bottomActive = readBottomSensor();
    
    // Debug output every second
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
    
    // Goal detection logic
    if (topActive && !topTriggered) {
        topTriggered = true;
        Serial.println("📊 Top sensor triggered - waiting for bottom...");
    }
    
    if (topTriggered && bottomActive) {
        updateScore(getCurrentScore() + 1);
        lastGoalTime = currentTime;
        topTriggered = false;
        
        Serial.print("🎯 GOAL DETECTED! New score: ");
        Serial.println(getCurrentScore());
        
        // Notify gateway via BLE
        sendScoreToBLE("goal");
        delay(100); // Small delay to prevent multiple detections
    }
}

void updateScore(int newScore) {
    // Update score in BLE handler
    setScore(newScore);
}

int getCurrentScore() {
    extern int score; // Access score from ble_handler.cpp
    return score;
}

bool isTopSensorActive() {
    return readTopSensor();
}

bool isBottomSensorActive() {
    return readBottomSensor();
}

void resetGoalDetection() {
    topTriggered = false;
    lastGoalTime = 0;
}