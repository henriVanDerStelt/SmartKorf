#ifndef KORFUNIT_H
#define KORFUNIT_H

#include <Arduino.h>

// Constants shared across modules
#define DEVICE_NAME "ESP32-BLE-Sender-1"
#define MPU_SDA 5   // GPIO8
#define MPU_SCL 6   // GPIO9

#define TOF_SDA 5   // GPIO6
#define TOF_SCL 6   // GPIO7
#define XSHUT_TOP 2
#define XSHUT_BOTTOM 3
#define TRIGGER_DISTANCE 300
#define GOAL_COOLDOWN 1000
#define PING_INTERVAL 3000
#define CONNECTION_TIMEOUT 10000

// Forward declarations for shared variables
extern int score;
extern bool deviceConnected;
extern bool connectionValid;
extern int pogingen;  // Add this

// Function declarations
void setupKorfUnit();
void loopKorfUnit();

#endif // KORFUNIT_H