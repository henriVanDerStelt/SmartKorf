#ifndef GOAL_DETECTOR_H
#define GOAL_DETECTOR_H

#include <Wire.h>
#include "korfunit.h"

// Conditional sensor support
#ifdef USE_SENSORS
#include <Adafruit_VL53L1X.h>
#endif

// Goal detection functions
void setupSensors();
void checkForGoal();
void updateScore(int newScore);
int getCurrentScore();
bool isTopSensorActive();
bool isBottomSensorActive();
void resetGoalDetection();
bool areSensorsEnabled();
void incrementAttempts();
int getAttempts();

#endif // GOAL_DETECTOR_H