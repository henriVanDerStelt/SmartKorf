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
  mxconfig.gpio.e  = E_PIN; // 1/32 scan panels only

  mxconfig.gpio.clk = CLK_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe  = OE_PIN;

  // change depending on usecase
  scoreBoardSettings();
  // funSettings();

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  if (!dma_display->begin()) {
    Serial.println("****** I2S/DMA init FAILED (memory or pin issue) ********");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("Successfully started matrix display.");

  dma_display->setBrightness8(128);
  dma_display->clearScreen();
}

void scoreBoardSettings(){
    /* 
  ALS JE DEZE SETTINGS HIERONDER AANPAST KRIJG JE SCREENTEARING/RUIS!
  DUS: ADMIN SETTINGS, NIET AANPASSEN TENZIJ JE WEET WAT JE DOET!
  */
  mxconfig.setPixelColorDepthBits(3); // 6 bits per color channel so 18 bits, lower if memory issues occur
  mxconfig.latch_blanking = 1; // the time between clocking data to the panel and then turning the LEDS 'on', 1 is default
  mxconfig.i2sspeed       = HUB75_I2S_CFG::HZ_15M; // (HZ_15M works best atm)
  mxconfig.clkphase       = true;      // or false depending on panel; default is true in newer versions
  mxconfig.double_buff = true;   // can be used for syncing to avoid visual artefacts, but uses more RAM. use with flipdmabuffer(). Also breaks site, no ram?
  mxconfig.min_refresh_rate = 120; // set to 120Hz minimum refresh rate (120 works best atm)

}

void funSettings(){
  /*
  MET DEZE SETTINGS KAN DE ESP32 ALLEEN HET DISPLAY AAN, GEEN MEMORY OVER VOOR ANDERE DINGEN!
  */
  mxconfig.setPixelColorDepthBits(8); 
  mxconfig.latch_blanking = 1; 
  mxconfig.i2sspeed       = HUB75_I2S_CFG::HZ_15M; 
  mxconfig.clkphase       = true;     
  mxconfig.double_buff = true;   
  mxconfig.min_refresh_rate = 60; 
}
