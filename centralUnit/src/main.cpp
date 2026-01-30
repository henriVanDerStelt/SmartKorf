#include <Arduino.h>
#include <website.h>
#include <display.h>
#include <png.h>
#include <buttons.h>

void setup(){
  Serial.begin(115200);
  delay(1000);
  displayInit();
  bleInit();
  buttons_begin();
  // pngInit();
  // listFiles();
}

void loop(){
  handleBluetooth();
  sendScoreNew();
  renderScreen();
  buttons_task();

  uint8_t changed = buttons_consumeChanged();
  if (changed) {
    if (changed & (1 << 0) && button_isPressed(0)) {
      scoreButton(2, 1); // away +
      Serial.println("AWAY +");
    }
    if (changed & (1 << 1) && button_isPressed(1)) {
      scoreButton(2, -1); // away -
      Serial.println("AWAY -");
    }
    if (changed & (1 << 2) && button_isPressed(2)) {
      timerReset();
      Serial.println("RESET");
    }
    if (changed & (1 << 3) && button_isPressed(3)) {
      bool time = timerIsPaused();
      if (time == true){
        Serial.println("TIMER STARTING!");
        Serial.print("Current timer paused state: ");
        Serial.println(time);
        timerStart();
      }
      if (time == false){
        Serial.println("TIMER STOPPING!");
        timerStop();
      }
      Serial.println("START / STOP");
    }
    if (changed & (1 << 4) && button_isPressed(4)) {
      scoreButton(1, 1); // home +
      Serial.println("HOME +");
    }
    if (changed & (1 << 5) && button_isPressed(5)) {
      scoreButton(1, -1); // home -
      Serial.println("HOME -");
    }
  }
  // drawPNG("/monkey.png", 0, 0);
  // dma_display->flipDMABuffer();
  // drawColors();
}