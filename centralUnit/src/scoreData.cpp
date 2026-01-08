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

// define globals exactly once:
int homeScore = 0;
int awayScore = 0;

BluetoothSerial SerialBT;      // only if you use BT classic
BLECharacteristic* pScoreChar = nullptr;

void sendScore() {
  static uint32_t lastUpdate = 0;
  uint32_t now = millis();

  if (now - lastUpdate < 500) return;
  lastUpdate = now;

  homeScore += random(0, 2);
  awayScore += random(0, 2);

  if (homeScore > 99) homeScore = 99;
  if (awayScore > 99) awayScore = 99;

  String jsonData = "{\"home\":" + String(homeScore) + ",\"away\":" + String(awayScore) + "}";

  if (pScoreChar) {
    // NimBLE: safest is explicit bytes + length
    pScoreChar->setValue((uint8_t*)jsonData.c_str(), jsonData.length());
    pScoreChar->notify(true);   // true = notify all subscribed clients (works fine even with 1)
  }

  Serial.print("ScoreBoard data sent: ");
  Serial.println(jsonData);
}


// ---- GAME TIMER (30 min) ----
static const uint32_t TIMER_TOTAL_MS = 30UL * 60UL * 1000UL;

static uint32_t g_timerStartMs = 0;     // "virtual start" time
static uint32_t g_timerPausedAtMs = 0;  // when pause was pressed
static bool     g_timerPaused = false;
static bool     g_timerInit = false;

static void timerEnsureInit() {
  if (!g_timerInit) {
    g_timerStartMs = millis();
    g_timerPaused = false;
    g_timerInit = true;
  }
}

// returns remaining seconds (0..)
uint32_t getRemainingTimerSeconds() {
  timerEnsureInit();

  uint32_t now = millis();
  uint32_t elapsed = g_timerPaused ? (g_timerPausedAtMs - g_timerStartMs)
                                   : (now - g_timerStartMs);

  if (elapsed >= TIMER_TOTAL_MS) return 0;
  return (TIMER_TOTAL_MS - elapsed) / 1000UL;
}

// Pause or resume
void timerPause(bool pause) {
  timerEnsureInit();

  if (pause && !g_timerPaused) {
    g_timerPaused = true;
    g_timerPausedAtMs = millis();
  } else if (!pause && g_timerPaused) {
    // shift start forward by the paused duration
    uint32_t now = millis();
    uint32_t pausedDuration = now - g_timerPausedAtMs;
    g_timerStartMs += pausedDuration;
    g_timerPaused = false;
  }
}

// Convenience toggle
void timerTogglePause() {
  timerPause(!g_timerPaused);
}

bool timerIsPaused() {
  timerEnsureInit();
  return g_timerPaused;
}

// Reset back to full duration
void timerReset() {
  g_timerStartMs = millis();   // start counting from NOW
  g_timerPausedAtMs = 0;
  g_timerPaused = false;
  g_timerInit = true;
}

// --- helper: two digits for scores ---
static inline String twoDigit(int v) {
  if (v < 10) return "0" + String(v);
  return String(v);
}

void renderScreen() {
  static int lastHome = -1, lastAway = -1;
  static uint32_t lastTimerSec = 0xFFFFFFFF;

  uint32_t timerSec = getRemainingTimerSeconds();

  bool scoreChanged = (homeScore != lastHome) || (awayScore != lastAway);
  bool timerChanged = (timerSec != lastTimerSec);

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

  drawLogo();
  // drawPenis();

  // dma_display->drawLine(63, 0, 63, 63, dma_display->color565(255, 0, 0)); //allignmentline 
  // dma_display->drawLine(64, 0, 64, 63, dma_display->color565(255, 0, 0)); //allignmentline 

  dma_display->flipDMABuffer();
}

void drawLogo() {
  // Colors
  // uint16_t black  = dma_display->color565(255, 255, 255);
  uint16_t blue = dma_display->color565(0, 0, 255);   // ball
  uint16_t yellow  = dma_display->color565(255, 255, 0);  // yellow
  uint16_t black = dma_display->color565(0, 0, 0);     // outline
  uint16_t purple   = dma_display->color565(128, 128, 128);   // grey

  //basket
  dma_display->fillRect(51, 0, 26, 34, black); // white background
  dma_display->drawRect(52, 2, 18, 10, purple); // outline
  dma_display->fillRect(53, 3, 16, 8, yellow); // basket infill
  dma_display->drawLine(53, 4, 65, 4, purple); // horizontal line
  dma_display->drawLine(53, 6, 65, 6, purple); // horizontal line
  dma_display->drawLine(53, 8, 65, 8, purple); // horizontal line

  //pole
  dma_display->drawRect(71, 10, 4, 24, purple);   // outline
  dma_display->fillRect(72, 11, 2, 23, black);   // blue rectangle

  //corner
  dma_display->drawRect(69, 4, 6, 6, purple);   // outline
  dma_display->fillRect(70, 5, 2, 4, yellow);   // yellow rectangle
  dma_display->fillRect(72, 5, 2, 5, yellow);   // yellow rectangle

  //speedlines
  dma_display->drawLine(56, 13, 56, 16, purple); // line1
  dma_display->drawLine(60, 13, 60, 15, purple); // line2
  dma_display->drawLine(64, 13, 64, 16, purple); // line3

  //ball
  dma_display->drawCircle(60, 25, 7.5, purple); // outline
  dma_display->fillCircle(60, 25, 6.5, blue); // blue infill
  
  //ball tints
  dma_display->drawPixel(56, 20, yellow); 
  dma_display->drawPixel(57, 20, yellow); 
  dma_display->drawPixel(55, 21, yellow); 
  dma_display->drawPixel(55, 22, yellow); 
  dma_display->drawPixel(56, 21, yellow);  

  dma_display->drawPixel(63, 20, yellow); 
  dma_display->drawPixel(64, 20, yellow); 
  dma_display->drawPixel(64, 21, yellow); 
  dma_display->drawPixel(65, 21, yellow); 
  dma_display->drawPixel(65, 22, yellow); 

  dma_display->drawPixel(57, 22, yellow); 
  dma_display->drawPixel(58, 22, yellow); 
  dma_display->drawPixel(59, 22, yellow); 
  dma_display->drawPixel(58, 21, yellow); 
  dma_display->drawPixel(59, 21, yellow); 
  dma_display->drawPixel(57, 23, yellow); 
  dma_display->drawPixel(58, 23, yellow); 

  dma_display->drawPixel(55, 24, yellow); 
  dma_display->drawPixel(56, 24, yellow); 
  dma_display->drawPixel(55, 25, yellow); 
  dma_display->drawPixel(56, 25, yellow); 
  dma_display->drawPixel(57, 25, yellow); 
  dma_display->drawPixel(56, 26, yellow); 
  dma_display->drawPixel(57, 26, yellow); 

  dma_display->drawPixel(61, 21, yellow);
  dma_display->drawPixel(62, 21, yellow);
  dma_display->drawPixel(61, 22, yellow);
  dma_display->drawPixel(62, 22, yellow);
  dma_display->drawPixel(63, 22, yellow);
  dma_display->drawPixel(62, 23, yellow);
  dma_display->drawPixel(63, 23, yellow);

  dma_display->drawPixel(64, 24, yellow);
  dma_display->drawPixel(65, 24, yellow);
  dma_display->drawPixel(63, 25, yellow);
  dma_display->drawPixel(64, 25, yellow);
  dma_display->drawPixel(65, 25, yellow);
  dma_display->drawPixel(63, 26, yellow);
  dma_display->drawPixel(64, 26, yellow);

  dma_display->drawPixel(60, 26, yellow);
  dma_display->drawPixel(60, 27, yellow);
  dma_display->drawPixel(60, 28, yellow);
  dma_display->drawPixel(60, 29, yellow);
  dma_display->drawPixel(59, 27, yellow);
  dma_display->drawPixel(59, 28, yellow);
  dma_display->drawPixel(59, 29, yellow);
  dma_display->drawPixel(61, 27, yellow);
  dma_display->drawPixel(61, 28, yellow);
  dma_display->drawPixel(61, 29, yellow);
  dma_display->drawPixel(58, 28, yellow);
  dma_display->drawPixel(62, 28, yellow);

  dma_display->drawPixel(54, 26, yellow);
  dma_display->drawPixel(54, 27, yellow);
  dma_display->drawPixel(55, 27, yellow);
  dma_display->drawPixel(55, 28, yellow);
  dma_display->drawPixel(55, 29, yellow);
  dma_display->drawPixel(56, 30, yellow);

  dma_display->drawPixel(66, 26, yellow);
  dma_display->drawPixel(66, 27, yellow);
  dma_display->drawPixel(65, 27, yellow);
  dma_display->drawPixel(65, 28, yellow);
  dma_display->drawPixel(65, 29, yellow);
  dma_display->drawPixel(64, 30, yellow);

  dma_display->drawPixel(58, 31, yellow);
  dma_display->drawPixel(59, 31, yellow);
  dma_display->drawPixel(60, 31, yellow);
  dma_display->drawPixel(61, 31, yellow);
  dma_display->drawPixel(62, 31, yellow);

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