#ifndef GOAL_DETECTOR_H
#define GOAL_DETECTOR_H

#include <Wire.h>
#include <Adafruit_VL53L1X.h>
#include "korfunit.h"

// Goal detection functions
void setupSensors();
void checkForGoal();
void updateScore(int newScore);
int getCurrentScore();
bool isTopSensorActive();
bool isBottomSensorActive();
void resetGoalDetection();

#endif // GOAL_DETECTOR_H