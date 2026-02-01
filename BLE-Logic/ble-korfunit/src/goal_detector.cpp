#include "goal_detector.h"
#include "ble_handler.h"
#include "ImpactDetection.h"

// Sensor objects - only create if sensors are enabled
#ifdef USE_SENSORS
static VL53L1X tof_top;      // Changed from Adafruit_VL53L1X
static VL53L1X tof_bottom;   // Changed from Adafruit_VL53L1X
#else
// Dummy sensor objects or pointers
static void* tof_top = nullptr;
static void* tof_bottom = nullptr;
#endif

// Goal detection state
static bool topTriggered = false;
static unsigned long lastGoalTime = 0;
static unsigned long lastDebugTime = 0;
static bool sensorsEnabled = false;
static unsigned long topTriggerTime = 0;
static bool shotDetectedByModel = false;

// Attempts counter
int pogingen = 0;
static unsigned long lastTopTriggerTime = 0;
static const unsigned long SHOT_DETECTION_WINDOW = 2000; // 2 seconds window for model detection

void incrementAttempts() {
    pogingen++;
    Serial.print("📊 Attempts incremented: ");
    Serial.println(pogingen);
    
    // Send attempts to BLE if connected
    if (isDeviceConnected()) {
        String message = "{\"pogingen\":" + String(pogingen) + ",\"type\":\"attempt\"}";
        // Note: You'll need to create a BLE function for this
    }
}

int getAttempts() {
    return pogingen;
}

void setAttempts(int newAttempts) {
    pogingen = newAttempts;
    Serial.print("📊 Attempts set by gateway: ");
    Serial.println(pogingen);
}
void setupSensors() {
    #ifdef USE_SENSORS
    Serial.println("Initializing VL53L1X sensors with Pololu library...");
    
    // Initialize XSHUT pins
    Serial.print("Initializing XSHUT pins - TOP: ");
    Serial.print(XSHUT_TOP);
    Serial.print(", BOTTOM: ");
    Serial.println(XSHUT_BOTTOM);
    
    pinMode(XSHUT_TOP, OUTPUT);
    pinMode(XSHUT_BOTTOM, OUTPUT);
    
    // Set both XSHUT LOW to power down sensors
    digitalWrite(XSHUT_TOP, LOW);
    digitalWrite(XSHUT_BOTTOM, LOW);
    delay(100);
    
    // Power up TOP sensor first with custom address
    digitalWrite(XSHUT_TOP, HIGH);
    delay(100);
    
    Serial.println("Initializing TOP sensor...");
    if (!tof_top.init()) {
        Serial.println("Failed to initialize TOP sensor!");
        sensorsEnabled = false;
        return;
    }
    tof_top.setAddress(0x30);  // Set custom address
    tof_top.setDistanceMode(VL53L1X::Long);
    tof_top.setMeasurementTimingBudget(50000);
    tof_top.startContinuous(50);
    Serial.println("Top sensor initialized at 0x30");
    
    // Power up BOTTOM sensor with different address
    digitalWrite(XSHUT_BOTTOM, HIGH);
    delay(100);
    
    Serial.println("Initializing BOTTOM sensor...");
    if (!tof_bottom.init()) {
        Serial.println("Failed to initialize BOTTOM sensor!");
        sensorsEnabled = false;
        return;
    }
    tof_bottom.setAddress(0x31);  // Set custom address
    tof_bottom.setDistanceMode(VL53L1X::Long);
    tof_bottom.setMeasurementTimingBudget(50000);
    tof_bottom.startContinuous(50);
    Serial.println("Bottom sensor initialized at 0x31");
    
    sensorsEnabled = true;
    Serial.println("✅ Sensors initialized with Pololu library!");
    #else
    Serial.println("Sensor support disabled at compile time");
    sensorsEnabled = false;
    #endif
}
#ifdef USE_SENSORS
static bool readTopSensor() {
    if (tof_top.dataReady()) {
        int distance = tof_top.read(false);  // false = don't wait for new data
        if (distance > 0 && distance < 4000) {
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}

static bool readBottomSensor() {
    if (tof_bottom.dataReady()) {
        int distance = tof_bottom.read(false);  // false = don't wait for new data
        if (distance > 0 && distance < 4000) {
            return distance < TRIGGER_DISTANCE;
        }
    }
    return false;
}
#else
// Dummy implementations when sensors disabled
static bool readTopSensor() { return false; }
static bool readBottomSensor() { return false; }
#endif

// Add this function to be called from ImpactDetection when a shot is detected
void notifyShotDetectedByModel() {
    shotDetectedByModel = true;
    Serial.println("🎯 Shot detected by Edge Impulse model");
}

void checkForGoal() {
    // If sensors are not enabled, don't try to detect goals
    if (!sensorsEnabled) {
        return;
    }
    
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
        #ifdef USE_SENSORS
        if (tof_top.dataReady() && tof_bottom.dataReady()) {
            Serial.print("Sensors active - Top triggered: ");
            Serial.print(topTriggered ? "YES" : "NO");
            Serial.print(", Top active: ");
            Serial.print(topActive ? "YES" : "NO");
            Serial.print(", Bottom active: ");
            Serial.print(bottomActive ? "YES" : "NO");
            Serial.print(", Attempts: ");
            Serial.println(pogingen);
        }
        #else
        Serial.println("Sensor simulation mode");
        #endif
    }
    
    // Check if shot was detected by model within the time window
    unsigned long timeSinceTopTrigger = currentTime - topTriggerTime;
    if (topTriggered && !shotDetectedByModel && timeSinceTopTrigger > SHOT_DETECTION_WINDOW) {
        // Top triggered but no shot detected by model within time window
        Serial.println("⚠️ Top triggered but no shot detected by model - counting as attempt");
        incrementAttempts();
        topTriggered = false;
        shotDetectedByModel = false;
        return;
    }
    
    // Reset shot detection flag if time window passed
    if (topTriggered && timeSinceTopTrigger > SHOT_DETECTION_WINDOW) {
        shotDetectedByModel = false;
    }
    
    // Goal detection logic
    if (topActive && !topTriggered) {
        topTriggered = true;
        topTriggerTime = currentTime;
        Serial.println("Top sensor triggered - waiting for bottom...");
    }
    
    // If top triggered but not bottom, and model detects shot, count as attempt
    if (topTriggered && !bottomActive && shotDetectedByModel) {
        Serial.println("🎯 Shot detected but no goal - counting as attempt");
        incrementAttempts();
        topTriggered = false;
        shotDetectedByModel = false;
    }
    
    // Goal detection (both sensors triggered)
    if (topTriggered && bottomActive) {
        updateScore(getCurrentScore() + 1);
        lastGoalTime = currentTime;
        topTriggered = false;
        shotDetectedByModel = false;
        
        Serial.print("GOAL DETECTED! New score: ");
        Serial.println(getCurrentScore());
        
        // Send score and attempts to BLE
        String message = "{\"score\":" + String(getCurrentScore()) + 
                        ",\"pogingen\":" + String(pogingen) + 
                        ",\"type\":\"goal\"}";
        
        sendScoreToBLE("goal");
        delay(100); // Small delay to prevent multiple detections
    }
    
    // Reset top trigger if bottom is never detected after some time
    if (topTriggered && currentTime - topTriggerTime > 3000) { // 3 second timeout
        Serial.println("⏰ Top trigger timeout - resetting");
        topTriggered = false;
        shotDetectedByModel = false;
    }
}

void updateScore(int newScore) {
    setScore(newScore);
}

int getCurrentScore() {
    return score;
}

bool isTopSensorActive() {
    if (!sensorsEnabled) return false;
    return readTopSensor();
}

bool isBottomSensorActive() {
    if (!sensorsEnabled) return false;
    return readBottomSensor();
}

void resetGoalDetection() {
    topTriggered = false;
    lastGoalTime = 0;
    pogingen = 0;
    shotDetectedByModel = false;
}

bool areSensorsEnabled() {
    return sensorsEnabled;
}