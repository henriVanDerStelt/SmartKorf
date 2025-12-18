#ifndef DISPLAY_H
#define DISPLAY_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_WIDTH   64   // adjust to your panel
#define PANEL_HEIGHT  64   // 32 or 64
#define PANELS_NUMBER 2

// ---- PINS (GPIO numbers) ----
#define R1_PIN   23
#define G1_PIN   19
#define B1_PIN   18
#define R2_PIN    5
#define G2_PIN    4
#define B2_PIN   16

#define A_PIN    17
#define B_PIN    21
#define C_PIN    22
#define D_PIN    26
#define E_PIN    27   // needed for 1/32 scan

#define CLK_PIN  25
#define LAT_PIN  32
#define OE_PIN   33

extern MatrixPanel_I2S_DMA* dma_display;

void displayInit();
void renderScreen();
void scoreBoardSettings();
void funSettings();
void showTime();


void drawLogo();
void drawPenis();
void drawColors();

#endif // DISPLAY_H
