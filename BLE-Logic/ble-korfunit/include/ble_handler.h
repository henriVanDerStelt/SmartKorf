#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

// Service UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// BLE Callback classes
class GatewayCommandCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic);
};

class MyServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer);
};

// BLE management functions
void setupBLE();
void loopBLE();
void sendScoreToBLE(const char* type);
void sendPing();
void checkConnectionHealth();

// Accessor functions for BLE state
bool isDeviceConnected();
bool isConnectionValid();
void setScore(int newScore);

#endif // BLE_HANDLER_H