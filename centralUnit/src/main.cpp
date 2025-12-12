#include <Arduino.h>
#include <website.h>
#include <display.h>

void setup(){
  Serial.begin(115200);
  delay(1000);
  websiteSetup();
  displaySetup();
}

void loop(){
  sendScore();
  renderScreen();
  // showTime();
  // drawPenis();
  // drawColors();
}