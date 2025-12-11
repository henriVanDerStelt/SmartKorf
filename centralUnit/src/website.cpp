#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BluetoothSerial.h"
#include <website.h>

BluetoothSerial SerialBT;

// BLE server / services / characteristics
BLEServer*       pServer        = nullptr;
BLEService*      pUartService   = nullptr;
BLEService*      pScoreService  = nullptr;
BLECharacteristic* pUartRxChar  = nullptr;
BLECharacteristic* pScoreChar   = nullptr;

// scores voor scoreboard
int homeScore = 0;
int awayScore = 0;

// ---- Callback voor BLE UART writes (van je BLE-sender ESP) ----
class RXCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    std::string msg = pCharacteristic->getValue();
    if (msg.length() == 0) return;

    Serial.print("BLE UART received: ");
    Serial.println(msg.c_str());

    // Alleen doorsturen als er een BT Classic client is
    if (SerialBT.hasClient()) {
      SerialBT.println(msg.c_str());
      Serial.println("Forwarded to BluetoothSerial");
    } else {
      Serial.println("Geen BluetoothSerial client verbonden, niks doorgestuurd");
    }
  }
};

void websiteSetup() {
  Serial.println();
  Serial.println("Start centralUnit (BLE server + BT Classic + ScoreBoard)...");

  // --- Bluetooth Classic (Serial over BT) ---
  if (!SerialBT.begin("centralUnit")) {
    Serial.println("BluetoothSerial start FAILED");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("BluetoothSerial started as 'centralUnit'");

  // optioneel: random seed voor demo-score updates
  randomSeed(analogRead(0));

  // --- BLE init ---
  BLEDevice::init("centralUnit");

  pServer = BLEDevice::createServer();

  // --- BLE UART service (voor jouw BLE sender ESP) ---
  pUartService = pServer->createService(SERVICE_UUID_UART);

  pUartRxChar = pUartService->createCharacteristic(
    CHARACTERISTIC_UUID_UART,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pUartRxChar->setCallbacks(new RXCallback());
  pUartService->start();

  // --- BLE ScoreBoard service (voor GitHub Pages / Web Bluetooth) ---
  pScoreService = pServer->createService(SERVICE_UUID_SCORE);

  pScoreChar = pScoreService->createCharacteristic(
    CHARACTERISTIC_UUID_SCORE,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |   // mag, als je PWA later terug wil schrijven
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Descriptor nodig voor notify in veel Web Bluetooth clients
  pScoreChar->addDescriptor(new BLE2902());

  pScoreService->start();

  // --- Advertising voor beide services ---
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID_UART);
  pAdvertising->addServiceUUID(SERVICE_UUID_SCORE);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE Server ready, advertising started.");
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
