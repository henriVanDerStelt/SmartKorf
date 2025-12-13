#include <cstddef>
#include <pngle.h>
#include <LittleFS.h>
#include <display.h>
#include <png.h>

// draw offset
static int png_offset_x = 0;
static int png_offset_y = 0;

void pngInit(){
    if (!LittleFS.begin(true)) {   // true = format if mount fails
    Serial.println("LittleFS mount failed!");
    while (true) delay(1000);
  }
  Serial.println("LittleFS mounted successfully.");
}

void listFiles() {
  File root = LittleFS.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Root is not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  if (!file) {
    Serial.println("No files found in LittleFS");
  }

  while (file) {
    Serial.print("FS file: ");
    Serial.print(file.name());
    Serial.print("  size=");
    Serial.println((unsigned)file.size());
    file = root.openNextFile();
  }

  root.close();
}


// pngle draw callback (called per pixel)
static void png_draw_cb(pngle_t* png,
                        uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h,
                        const uint8_t* rgba) {

  uint16_t color = dma_display->color565(
    rgba[0], rgba[1], rgba[2]
  );

  dma_display->drawPixel(
    png_offset_x + x,
    png_offset_y + y,
    color
  );
}

bool drawPNG(const char* path, int x, int y) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.println("PNG open failed");
    return false;
  }

  png_offset_x = x;
  png_offset_y = y;

  pngle_t* png = pngle_new();
  pngle_set_draw_callback(png, png_draw_cb);

  uint8_t buf[128];
  while (f.available()) {
    int len = f.read(buf, sizeof(buf));
    pngle_feed(png, buf, len);
  }

  pngle_destroy(png);
  f.close();
  return true;
}
