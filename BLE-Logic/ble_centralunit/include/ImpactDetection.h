
#ifndef IMPACTDETECTION_H
#define IMPACTDETECTION_H
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <array>

class ImpactDetection {
public:
    ImpactDetection();

    void begin();
    void update();

private:
    // ================= WIFI =================
    const char* wifiSsid;
    const char* wifiPass;
    const char* laptopIp;
    int         serverPort;
    String      serverUrl;

    int pogingen;

    // ================= PINS =================
    static constexpr int ANALOG_PIN  = D0;
    static constexpr int DIGITAL_PIN = D7;

    // ================= MPU =================
    Adafruit_MPU6050 mpu;
    sensors_event_t accel, gyro, temp;

    // ================= SAMPLING =================
    static constexpr uint32_t EI_SAMPLE_INTERVAL_MS = 53;
    static constexpr uint32_t RECORD_SAMPLE_RATE_HZ = 200;
    static constexpr uint32_t RECORD_SAMPLE_INTERVAL_MS =
        500 / RECORD_SAMPLE_RATE_HZ;

    // ================= EVENT LIMITS =================
    static constexpr float START_LIMIT = 3000.0f;
    static constexpr float STOP_LIMIT  = 3450.0f;

    // ================= EVENT STORAGE =================
    static constexpr int MAX_SAMPLES = 1000;
    float samples[MAX_SAMPLES][9];
    int   sampleIndex;
    bool  recording;

    // ================= EDGE IMPULSE =================
    float* features;
    size_t featureIx;

    uint32_t lastEiSampleTime;
    uint32_t lastRecordSampleTime;

    // ================= INTERNAL METHODS =================
    static int eiGetData(size_t offset, size_t length, float *out_ptr);

    std::array<float, 8> readSensors();

    void connectWiFi();
    void uploadCSV();
};


#endif // IMPACTDETECTION_H

