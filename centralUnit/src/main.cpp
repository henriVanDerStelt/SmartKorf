#include <Arduino.h>
#include <website.h>
#include <display.h>
#include <png.h>

void setup(){
  Serial.begin(115200);
  delay(1000);
  websiteSetup();
  displayInit();
  // pngInit();
  // listFiles();
}

void loop(){
  sendScore();
  renderScreen();
  // drawPNG("/monkey.png", 0, 0);
  // dma_display->flipDMABuffer();
  // drawColors();
}