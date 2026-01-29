
#ifndef IMPACTDETECTION_H
#define IMPACTDETECTION_H
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <array>

class ImpactDetection {
public:
    ImpactDetection();

    void begin();
    void update();
    void getPogingen(int pogingen);

private:

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

    // ================= EVENT STORAGE =================
    static constexpr int MAX_SAMPLES = 1000;
    float samples[MAX_SAMPLES][9];
    int   sampleIndex;
 

    // ================= EDGE IMPULSE =================
    float* features;
    size_t featureIx;

    uint32_t lastEiSampleTime;
    uint32_t lastRecordSampleTime;

    // ================= INTERNAL METHODS =================
    static int eiGetData(size_t offset, size_t length, float *out_ptr);

    std::array<float, 8> readSensors();

};


#endif // IMPACTDETECTION_H

