#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <display.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;

void displaySetup() {
  Serial.println("Starting HUB75 on LIVE Mini Kit ESP32...");

  HUB75_I2S_CFG mxconfig(
    PANEL_WIDTH,
    PANEL_HEIGHT,
    PANELS_NUMBER
  );

  // Pin mapping
  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;

  mxconfig.gpio.a  = A_PIN;
  mxconfig.gpio.b  = B_PIN;
  mxconfig.gpio.c  = C_PIN;
  mxconfig.gpio.d  = D_PIN;
  mxconfig.gpio.e  = E_PIN; // 1/32 scan panels only

  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe  = OE_PIN;
  mxconfig.latch_blanking = 16; // the time between clocking data to the panel and then turning the LEDS 'on', 4 is default
  mxconfig.i2sspeed       = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase       = true;      // or false depending on panel; default is true in newer versions

  mxconfig.double_buff = false;   // use double buffering for smoother animations
  mxconfig.min_refresh_rate = 60; // set to 120Hz minimum refresh rate

  Serial.println("Creating MatrixPanel_I2S_DMA...");
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  Serial.println("After constructor.");

  Serial.println("Calling begin()...");
  if (!dma_display->begin()) {
    Serial.println("****** I2S/DMA init FAILED (memory or pin issue) ********");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("After begin().");

  dma_display->setBrightness8(128);
  dma_display->clearScreen();
//   dma_display->drawPixel(0, 0, dma_display->color565(255, 255, 255));
//   dma_display->setCursor(2, 10);
//   dma_display->setTextSize(4);
//   dma_display->setTextColor(dma_display->color565(0, 255, 0));
//   dma_display->fillScreen(dma_display->color565(255, 255, 255));
//   dma_display->print("Remco");
//   dma_display->flipDMABuffer();
}
