#ifndef WEBSITE_H
#define WEBSITE_H

#include <BLECharacteristic.h>
#include <BluetoothSerial.h>   // <-- REQUIRED so extern BluetoothSerial works

// ---- UUIDs voor UART-bridge (BLE sender -> BT classic) ----
#define SERVICE_UUID_UART        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_UART "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// ---- UUIDs voor ScoreBoard (PWA / Web Bluetooth) ----
#define SERVICE_UUID_SCORE       "3bd083ef-9c40-4fd1-992f-d0450276a783"
#define CHARACTERISTIC_UUID_SCORE "a50704f1-ba55-44cf-96ec-2de6ded239d4"

extern int homeScore;
extern int awayScore;

extern BluetoothSerial SerialBT;     // Now the type is known everywhere
extern BLECharacteristic* pScoreChar;

void websiteSetup();
void sendScore();

#endif // WEBSITE_H
