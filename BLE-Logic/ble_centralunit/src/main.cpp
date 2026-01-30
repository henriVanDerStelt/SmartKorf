#include <Arduino.h>
#include "ImpactDetection.h"

ImpactDetection impact;

void setup() {
    impact.begin();
}

void loop() {
    impact.update();
}