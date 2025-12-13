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

// --- helper: two digits for scores ---
static inline String twoDigit(int v) {
  if (v < 10) return "0" + String(v);
  return String(v);
}

// --- match timer: 30:00 countdown from boot ---
static inline void formatMatchTime(char out[6]) {
  static uint32_t startMs = millis();              // set once at first call
  const uint32_t totalMs = 30UL * 60UL * 1000UL;   // 30 minutes

  uint32_t elapsed = millis() - startMs;
  uint32_t remainingMs = (elapsed >= totalMs) ? 0 : (totalMs - elapsed);

  uint32_t sec = remainingMs / 1000UL;
  uint32_t mm = sec / 60UL;
  uint32_t ss = sec % 60UL;

  snprintf(out, 6, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

void renderScreen() {
  static int lastHome = -1, lastAway = -1;
  static uint32_t lastTimerSec = 0xFFFFFFFF;

  // compute timer seconds remaining
  static uint32_t startMs = millis();              // timer starts at boot (first render)
  const uint32_t totalMs = 30UL * 60UL * 1000UL;
  uint32_t elapsed = millis() - startMs;
  uint32_t remainingMs = (elapsed >= totalMs) ? 0 : (totalMs - elapsed);
  uint32_t timerSec = remainingMs / 1000UL;

  bool scoreChanged = (homeScore != lastHome) || (awayScore != lastAway);
  bool timerChanged = (timerSec != lastTimerSec);

  // Only redraw when needed (either timer tick or score change)
  if (!scoreChanged && !timerChanged) return;

  lastHome = homeScore;
  lastAway = awayScore;
  lastTimerSec = timerSec;

  dma_display->clearScreen();
  dma_display->setTextColor(dma_display->color565(255, 0, 0)); //red color

  // ---- HOMESCORE ----
  dma_display->setTextSize(4);
  dma_display->setCursor(4, 11);
  dma_display->print(twoDigit(homeScore));

  // ---- AWAYSCORE ----
  dma_display->setTextSize(4);
  dma_display->setCursor(80, 11);
  dma_display->print(twoDigit(awayScore));

  // ---- TIMER ----
  char tbuf[6];
  uint32_t mm = timerSec / 60UL;
  uint32_t ss = timerSec % 60UL;
  snprintf(tbuf, sizeof(tbuf), "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);

  // ---- TIMENUMBERS ----
  dma_display->setTextSize(2);
  dma_display->setCursor(35, 47);   
  dma_display->print(tbuf);

  //lines and words
  // ---- HOME ----
  dma_display->setTextSize(1);
  dma_display->setCursor(14, 1);
  dma_display->print("HOME");

    // ---- AWAY ----
  dma_display->setTextSize(1);
  dma_display->setCursor(90, 1);
  dma_display->print("AWAY");

  // ---- TIME ----
  dma_display->setTextSize(1);
  dma_display->setCursor(52, 37);
  dma_display->print("TIME");

  // ---- LINES ----
  dma_display->drawLine(0, 45, 50, 45, dma_display->color565(255, 0, 0)); //underline
  dma_display->drawLine(77, 45, 127, 45, dma_display->color565(255, 0, 0)); //underline
  dma_display->drawLine(50, 0, 50, 44, dma_display->color565(255, 0, 0)); //vertical line left
  dma_display->drawLine(77, 0, 77, 44, dma_display->color565(255, 0, 0)); //vertical line right
  dma_display->drawLine(51, 34, 76, 34, dma_display->color565(255, 0, 0)); //horizontal line above time

  drawPenis();

  // dma_display->drawLine(63, 0, 63, 63, dma_display->color565(255, 0, 0)); //allignmentline 
  // dma_display->drawLine(64, 0, 64, 63, dma_display->color565(255, 0, 0)); //allignmentline 

  dma_display->flipDMABuffer();
}

void drawPenis() {
  dma_display->fillCircle(59, 26, 5, dma_display->color565(255, 0, 0)); //ball
  dma_display->fillCircle(68, 26, 5, dma_display->color565(255, 0, 0)); //ball
  dma_display->fillRect(60, 8, 8, 15, dma_display->color565(255, 0, 0)); // shaft
  dma_display->fillCircle(64, 7, 5, dma_display->color565(255, 0, 0)); //head
  dma_display->fillCircle(63, 7, 5, dma_display->color565(255, 0, 0)); //head 
  dma_display->fillRect(63, 0, 2, 3, dma_display->color565(0, 0, 0)); // cutout
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