#include <Arduino.h>
#include <website.h>
#include <display.h>

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

void renderScreen() {
  // Clear the display each refresh
  dma_display->clearScreen();

  // ---------------- HOME ----------------
  dma_display->setTextColor(dma_display->color565(255, 255, 255));

  dma_display->setTextSize(1);
  dma_display->setCursor(10, 5);
  dma_display->print("HOME");

  dma_display->setTextSize(4);
  dma_display->setCursor(10, 25);
  dma_display->print(homeScore);   // <-- always shows newest random value

  // ---------------- GUEST ----------------
  dma_display->setTextSize(1);
  dma_display->setCursor(60, 5);
  dma_display->print("GUEST");

  dma_display->setTextSize(4);
  dma_display->setCursor(60, 25);
  dma_display->print(awayScore);   // <-- always shows newest random value
}

void drawPenis() {
    dma_display->drawCircle(80, 20, 10, dma_display->color565(255, 0, 0)); //ball
    dma_display->drawCircle(80, 40, 10, dma_display->color565(255, 0, 0)); //ball
    dma_display->drawRect(30, 25, 50, 15, dma_display->color565(255, 0, 0)); // Lichaam
    dma_display->drawCircle(30, 32.5, 7.5, dma_display->color565(255, 0, 0)); //ball

}
