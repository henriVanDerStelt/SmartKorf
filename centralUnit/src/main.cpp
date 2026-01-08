#include <Arduino.h>
#include <website.h>
#include <display.h>
#include <png.h>

void setup(){
  Serial.begin(115200);
  delay(1000);
  displayInit();
  bleInit();
  // pngInit();
  // listFiles();
}

void loop(){
  handleBluetooth();
  sendScore();
  renderScreen();
  // drawPNG("/monkey.png", 0, 0);
  // dma_display->flipDMABuffer();
  // drawColors();
}