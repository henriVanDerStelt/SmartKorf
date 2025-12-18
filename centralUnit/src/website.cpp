#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BluetoothSerial.h"
#include <website.h>

BluetoothSerial SerialBT;

// BLE server / services / characteristics
BLEServer*          pServer       = nullptr;
BLEService*         pUartService  = nullptr;
BLEService*         pScoreService = nullptr;
BLECharacteristic*  pUartRxChar   = nullptr;
BLECharacteristic*  pScoreChar    = nullptr;

// scores voor scoreboard
int homeScore = 0;
int awayScore = 0;

// ---- Callback voor BLE UART writes (van je BLE-sender ESP) ----
class RXCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    String msg = String(value.c_str());

    if (msg.length() == 0) return;

    Serial.print("BLE UART received from sender ESP: ");
    Serial.println(msg);

    if (SerialBT.hasClient()) {
      SerialBT.println(msg);
      Serial.println("Forwarded to BluetoothSerial");
    } else {
      Serial.println("No BT Classic client connected");
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("BLE client connected (sender ESP or website)");
  }
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("BLE client disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted.");
  }
};

// --- 1) BT Classic init (optional separate) ---
void btClassicInit() {
  Serial.println("[BT] Starting BluetoothSerial...");
  if (!SerialBT.begin("centralUnit")) {
    Serial.println("[BT] BluetoothSerial start FAILED");
    while (true) delay(1000);
  }
  Serial.println("[BT] BluetoothSerial started as 'centralUnit'");
}

// --- Call once before any BLE init functions ---
void bleCoreInit() {
  static bool inited = false;
  if (inited) return;
  inited = true;

  Serial.println("[BLE] Core init...");
  BLEDevice::init("centralUnit");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
}

// --- 2) Sender receiver BLE init (UART write characteristic) ---
void senderBleInit() {
  bleCoreInit();

  Serial.println("[BLE] Init sender UART service...");
  pUartService = pServer->createService(SERVICE_UUID_UART);

  pUartRxChar = pUartService->createCharacteristic(
    CHARACTERISTIC_UUID_UART,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pUartRxChar->setCallbacks(new RXCallback());

  pUartService->start();
  Serial.println("[BLE] Sender UART service started.");
}

// --- 3) Website BLE init (ScoreBoard READ/NOTIFY characteristic) ---
void siteBleInit() {
  bleCoreInit();

  Serial.println("[BLE] Init website ScoreBoard service...");
  pScoreService = pServer->createService(SERVICE_UUID_SCORE);

  pScoreChar = pScoreService->createCharacteristic(
    CHARACTERISTIC_UUID_SCORE,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pScoreChar->addDescriptor(new BLE2902());
  pScoreService->start();

  Serial.println("[BLE] Website ScoreBoard service started.");
}

// --- Advertising: call after whichever services you enabled ---
void bleStartAdvertising(bool advertiseSender = true, bool advertiseSite = true) {
  Serial.println("[BLE] Starting advertising...");

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->stop();

  // IMPORTANT: re-add only what you want
  if (advertiseSender) {
    pAdvertising->addServiceUUID(SERVICE_UUID_UART);
  }
  if (advertiseSite) {
    pAdvertising->addServiceUUID(SERVICE_UUID_SCORE);
  }

  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.print("[BLE] Advertising started. sender=");
  Serial.print(advertiseSender);
  Serial.print(" site=");
  Serial.println(advertiseSite);
}


// --- Your old entrypoint can now just orchestrate ---
void websiteSetup() {
  Serial.println();
  Serial.println("Start centralUnit...");

  btClassicInit();

  // Pick what you want:
  senderBleInit();   // sender ESP path
  siteBleInit();     // website path

  // Advertise both (or toggle for debugging)
  bleStartAdvertising(true, true);

  Serial.println("[BLE] Ready. Waiting for clients...");
}

