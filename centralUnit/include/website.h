#ifndef WEBSITE_H
#define WEBSITE_H

#include <BLECharacteristic.h>
#include <BluetoothSerial.h>   // <-- REQUIRED so extern BluetoothSerial works

// BLE Configuration
#define DEVICE_NAME "ESP32-BLE-Gateway"

// Service UUIDs
#define SENDER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // From senders
#define SENDER_CHAR_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Service for PWA connection
#define GATEWAY_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_DATA_UUID "22345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_STATUS_UUID "32345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_COMMAND_UUID "42345678-1234-1234-1234-123456789abc"

extern int homeScore;
extern int awayScore;

extern BluetoothSerial SerialBT;     // Now the type is known everywhere
extern BLECharacteristic* pScoreChar;

uint32_t getRemainingTimerSeconds();

void websiteSetup();
void scoreRNG();
void sendScoreNew();

void changeNames(String newHome, String newAway);

void bleInit();
void handleBluetooth();

void timerReset();        // reset to 30:00
void timerPause(bool);    // pause
void timerTogglePause();  // toggle pause/resume


#endif // WEBSITE_H
