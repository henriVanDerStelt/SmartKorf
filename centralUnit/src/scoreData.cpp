#include <Arduino.h>
#include <website.h>
#include <display.h>
#include <FastLED.h>

uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void sendScore() {
  // Demo: elke 5 seconden random scores aanpassen en naar ScoreBoard characteristic sturen.
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate >= 5000) {
    lastUpdate = now;

    // willekeurige increment tussen 0-1
    homeScore += random(0, 2);
    awayScore += random(0, 2);

    // JSON payload voor je PWA:
    // {"home":3,"away":5}
    String jsonData = "{\"home\":" + String(homeScore) + ",\"away\":" + String(awayScore) + "}";

    if (pScoreChar != nullptr) {
      pScoreChar->setValue(jsonData.c_str());
      pScoreChar->notify();  // stuurt data naar de Web Bluetooth client
    }

    Serial.print("ScoreBoard data sent: ");
    Serial.println(jsonData);
  }

  // verder niks nodig; BLE callbacks en BluetoothSerial lopen op de achtergrond
}

String twoDigit(int value) {
  if (value < 10) return "0" + String(value);
  return String(value);
}

void renderScreen() {
  static int lastHome = -1, lastAway = -1;
  if (homeScore == lastHome && awayScore == lastAway) return;
  lastHome = homeScore;
  lastAway = awayScore;

  dma_display->clearScreen();
  dma_display->drawLine(0, 43, 128, 43, dma_display->color565(255, 0, 0));

  // ---------------- HOME ----------------
  dma_display->setTextColor(dma_display->color565(255, 0, 0));

  dma_display->setTextSize(1);
  dma_display->setCursor(14, 4);
  dma_display->print("HOME");

  dma_display->setTextSize(4);
  dma_display->setCursor(4, 14);
  dma_display->print(twoDigit(homeScore));   

  // ---------------- AWAY ----------------
  dma_display->setTextSize(1);
  dma_display->setCursor(90, 4);
  dma_display->print("AWAY");

  dma_display->setTextSize(4);
  dma_display->setCursor(80, 14);
  dma_display->print(twoDigit(awayScore));   

  // dma_display->flipDMABuffer();
}

void showTime(){
  //put match time here x axis = 45
}

void drawPenis() {
    dma_display->drawCircle(80, 20, 10, dma_display->color565(255, 0, 0)); //ball
    dma_display->drawCircle(80, 40, 10, dma_display->color565(255, 0, 0)); //ball
    dma_display->drawRect(30, 25, 50, 15, dma_display->color565(255, 0, 0)); // Lichaam
    dma_display->drawCircle(30, 32.5, 7.5, dma_display->color565(255, 0, 0)); //ball
}

static bool plasmaStarted = false;

void drawColors() {
  if (!plasmaStarted) {
    plasmaStarted = true;

    currentPalette = RainbowColors_p;
    Serial.println("Starting plasma effect...");
    fps_timer = millis();
  }
    for (int x = 0; x < 128; x++) {
            for (int y = 0; y <  64; y++) {
                int16_t v = 128;
                uint8_t wibble = sin8(time_counter);
                v += sin16(x * wibble * 3 + time_counter);
                v += cos16(y * (128 - wibble)  + time_counter);
                v += sin16(y * x * cos8(-time_counter) / 8);

                currentColor = ColorFromPalette(currentPalette, (v >> 8)); //, brightness, currentBlendType);
                dma_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
            }
    }

    ++time_counter;
    ++cycles;
    ++fps;

    if (cycles >= 1024) {
        time_counter = 0;
        cycles = 0;
        currentPalette = palettes[random(0,sizeof(palettes)/sizeof(palettes[0]))];
    }

    // print FPS rate every 5 seconds
    // Note: this is NOT a matrix refresh rate, it's the number of data frames being drawn to the DMA buffer per second
    if (fps_timer + 5000 < millis()){
      Serial.printf_P(PSTR("Effect fps: %d\n"), fps/5);
      fps_timer = millis();
      fps = 0;
    }
} // end loop