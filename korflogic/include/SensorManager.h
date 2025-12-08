#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Adafruit_VL53L1X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// Pin definities voor XIAO ESP32-C3
#define VIBRATION_PIN_DIGITAL D0  // DO pin van SW-18010P
#define VIBRATION_PIN_ANALOG D1   // AO pin van SW-18010P (was TOF_XSHUT_TOP)
#define TOF_XSHUT_TOP D2          // Verplaatst naar D2
#define TOF_XSHUT_BOT D3          // Verplaatst naar D3
#define SDA_PIN D4
#define SCL_PIN D5

// TOF I2C adressen (VL53L1X standaard 0x29)
#define TOF_TOP_ADDRESS 0x29
#define TOF_BOTTOM_ADDRESS 0x30

// Detectie parameters
#define BALL_DIAMETER_MM 220        // Korfbal diameter ~220mm
#define DETECTION_THRESHOLD_MM 300  // Detectieafstand
#define GYRO_THRESHOLD 1.5          // rad/s voor rotatie detectie
#define ACCEL_THRESHOLD 2.0         // m/s² voor impact detectie
#define VIBRATION_DIGITAL_THRESHOLD HIGH  // Digitale trigger level
#define VIBRATION_ANALOG_THRESHOLD 512    // Analoge threshold (0-4095 op ESP32)
#define VIBRATION_DEBOUNCE_MS 100   // Debounce tijd voor vibratie
#define TOF_TIMEOUT_MS 2000         // Timeout voor bal door korf

// Kies vibration sensor mode
#define USE_ANALOG_VIBRATION true   // true = gebruik AO pin, false = gebruik DO pin

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    void enterDeepSleep(uint64_t sleepTimeSeconds);
    
    // Getters voor sensor status
    bool isGoalScored() const { return goalScored; }
    bool isShotAttempt() const { return shotAttempt; }
    bool isPlayerCollision() const { return playerCollision; }
    void resetDetections();
    
    // Diagnostics
    void printStatus();
    uint16_t getVibrationLevel() const { return lastVibrationLevel; }
    
private:
    // Sensor objecten
    Adafruit_VL53L1X tofTop;
    Adafruit_VL53L1X tofBottom;
    Adafruit_MPU6050 mpu;
    
    // Status variabelen
    bool goalScored;
    bool shotAttempt;
    bool playerCollision;
    
    // Timing variabelen
    unsigned long topTriggerTime;
    unsigned long bottomTriggerTime;
    unsigned long lastVibrationTime;
    
    // Detectie flags
    bool topSensorTriggered;
    bool bottomSensorTriggered;
    
    // Vibration data
    uint16_t lastVibrationLevel;
    
    // Private methoden
    bool initializeTOF();
    bool initializeMPU();
    void checkTOFSensors();
    void checkIMU();
    void checkVibration();
    bool detectBallVsPlayerImpact(float accelMagnitude, float gyroMagnitude, uint16_t vibrationLevel);
    void configureLowPowerMode();
};

#endif // SENSOR_MANAGER_H