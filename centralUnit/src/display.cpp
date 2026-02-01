#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <display.h>

MatrixPanel_I2S_DMA *dma_display = nullptr;

HUB75_I2S_CFG mxconfig(
  PANEL_WIDTH,
  PANEL_HEIGHT,
  PANELS_NUMBER
);

void displayInit() {
  Serial.println("Starting LED matrix display...");

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
  mxconfig.gpio.e  = E_PIN;

  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe  = OE_PIN;

  // Use scoreboard settings by default
  scoreBoardSettings();

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  if (!dma_display->begin()) {
    Serial.println("I2S/DMA init failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("Matrix display started");

  dma_display->setBrightness8(128);
  dma_display->clearScreen();
}

void scoreBoardSettings(){
  mxconfig.setPixelColorDepthBits(3);
  mxconfig.latch_blanking = 1;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_15M;
  mxconfig.clkphase = true;
  mxconfig.double_buff = false;
  mxconfig.min_refresh_rate = 120;
}