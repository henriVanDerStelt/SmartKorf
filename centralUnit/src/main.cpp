#include <Arduino.h>
#include <website.h>

void setup(){
  Serial.begin(115200);
  delay(1000);
  websiteSetup();
}

void loop(){
  sendScore();
}