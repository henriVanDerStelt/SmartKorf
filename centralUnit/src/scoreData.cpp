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

  drawLogo();
  // drawPenis();

  // dma_display->drawLine(63, 0, 63, 63, dma_display->color565(255, 0, 0)); //allignmentline 
  // dma_display->drawLine(64, 0, 64, 63, dma_display->color565(255, 0, 0)); //allignmentline 

  dma_display->flipDMABuffer();
}

void drawLogo() {
  // Box bounds (inside your red lines)
  const int x0 = 51, x1 = 76;
  const int y0 = 0,  y1 = 33;

  // Colors
  uint16_t white  = dma_display->color565(255, 255, 255);
  uint16_t orange = dma_display->color565(255, 165, 0);   // ball
  uint16_t brown  = dma_display->color565(210, 170, 90);  // basket-ish

  // --- Basket (rim + net) ---
  // Rim (simple "oval" look via lines)
  int cx = 63;     // center x of logo
  int rimY = 19;   // rim height
  int rimL = 56;   // rim left
  int rimR = 70;   // rim right

  dma_display->drawLine(rimL, rimY, rimR, rimY, brown);          // top rim
  dma_display->drawLine(rimL+1, rimY+1, rimR-1, rimY+1, brown);  // rim thickness
  dma_display->drawLine(rimL, rimY, rimL+2, rimY-2, brown);      // left curve hint
  dma_display->drawLine(rimR-2, rimY-2, rimR, rimY, brown);      // right curve hint

  // Net (tapered down)
  int netTopL = rimL+2, netTopR = rimR-2;
  int netBotL = 59,     netBotR = 67;
  int netBotY = 30;

  dma_display->drawLine(netTopL, rimY+2, netBotL, netBotY, brown);
  dma_display->drawLine(netTopR, rimY+2, netBotR, netBotY, brown);
  dma_display->drawLine(netBotL, netBotY, netBotR, netBotY, brown);

  // Net “weave”
  dma_display->drawLine(netTopL+2, rimY+5, netBotL+1, netBotY-2, brown);
  dma_display->drawLine(netTopR-2, rimY+5, netBotR-1, netBotY-2, brown);
  dma_display->drawLine(netTopL+5, rimY+6, netTopR-5, rimY+6, brown);
  dma_display->drawLine(netTopL+3, rimY+10, netTopR-3, rimY+10, brown);
  dma_display->drawLine(netTopL+2, rimY+14, netTopR-2, rimY+14, brown);

  // --- Ball (entering + going through rim) ---
  // Put the ball slightly above rim and overlapping into net
  int ballX = 58;
  int ballY = 12;
  int r = 4;

  dma_display->fillCircle(ballX, ballY, r, orange);
  dma_display->drawCircle(ballX, ballY, r, white); // outline

  // Ball seams (simple)
  dma_display->drawLine(ballX - r + 1, ballY, ballX + r - 1, ballY, white);
  dma_display->drawLine(ballX, ballY - r + 1, ballX, ballY + r - 1, white);
  dma_display->drawLine(ballX - 2, ballY - 3, ballX + 2, ballY + 3, white);

  // --- Optional: tiny motion cue (makes it read as "going through") ---
  dma_display->drawLine(ballX - 8, ballY - 6, ballX - 5, ballY - 3, white);
  dma_display->drawLine(ballX - 7, ballY - 3, ballX - 4, ballY, white);
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