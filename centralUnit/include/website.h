#ifndef WEBSITE_H
#define WEBSITE_H

#include <BLECharacteristic.h>
#include <Arduino.h>

// BLE Configuration
#define DEVICE_NAME "ESP32-BLE-Gateway"

// Service UUIDs
#define SENDER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // From senders
#define SENDER_CHAR_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SENDER_CHAR_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// Service for PWA connection
#define GATEWAY_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_DATA_UUID "22345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_STATUS_UUID "32345678-1234-1234-1234-123456789abc"
#define GATEWAY_CHAR_COMMAND_UUID "42345678-1234-1234-1234-123456789abc"

// Service for ScoreBoard
#define SCOREBOARD_SERVICE_UUID "12345678-1234-1234-1234-1234567890AB"
#define SCOREBOARD_CHAR_UUID "ABCDEFAB-1234-1234-1234-1234567890AB"

// Global variables
extern int homeScore;
extern int awayScore;

// Function declarations
uint32_t getRemainingTimerSeconds();
uint32_t getRemainingTimerMs();  // Returns remaining time in milliseconds
String getFormattedTime();  // Returns time in mm:ss:msms format
void websiteSetup();
void scoreChange(int score, int who);
void sendScoreNew();  // This will now be implemented in website.cpp
void sendScoreToPWA(); // Helper function to send to PWA

void changeNames(String newHome, String newAway);
void bleInit();
void handleBluetooth();
void timerStart();
void timerStop();
void timerReset();
void timerPause(bool);
void timerTogglePause();
void setupScoreBoardBLE();

#endif // WEBSITE_H