#include "ImpactDetection.h"
#include "secret.h"

// EDGE IMPULSE — ONLY HERE
#include <edge-impulse-sdk/classifier/ei_run_classifier.h>


// Static pointer for EI callback
static ImpactDetection* instancePtr = nullptr;

ImpactDetection::ImpactDetection()
    : wifiSsid(WIFI_SSIDH),
      wifiPass(WIFI_PASSWORDH),
      laptopIp(LAPTOP_IPH),
      serverPort(SERVER_PORTH),
      serverUrl(String("http://") + LAPTOP_IPH + ":" + SERVER_PORTH + "/upload"),
      pogingen(0),
      sampleIndex(0),
      recording(false),
      featureIx(0),
      lastEiSampleTime(0),
      lastRecordSampleTime(0)
{
    static float featureBuffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
    features = featureBuffer;

    instancePtr = this;
}


void ImpactDetection::begin() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== ESP32-C3 Impact Detection ===");

    connectWiFi();

    Wire.begin(5, 6);
    delay(500);

    if (!mpu.begin(0x68, &Wire)) {
        Serial.println("MPU6050 NOT FOUND");
        return;
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("MPU6050 OK");
    ei_printf("EI frame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
}

void ImpactDetection::update() {
    uint32_t now = millis();

    // ---------- EDGE IMPULSE ----------
    if (now - lastEiSampleTime >= EI_SAMPLE_INTERVAL_MS) {
        lastEiSampleTime = now;

        auto frame = readSensors();

        for (float v : frame) {
            features[featureIx++] = v;
        }

        if (featureIx >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            signal_t signal;
            signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
            signal.get_data = eiGetData;

            ei_impulse_result_t result = {0};

            if (run_classifier(&signal, &result, false) == EI_IMPULSE_OK) {
                for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                    ei_printf("%s: %.3f\n",
                              result.classification[i].label,
                              result.classification[i].value);

                     if (strcmp(result.classification[i].label, "schot") == 0) {
                            if (result.classification[i].value > 0.5) {
                                pogingen++;
                                Serial.print("Poging nummer: ");
                                Serial.println(pogingen);
                            }
                        }
                }
#if EI_CLASSIFIER_HAS_ANOMALY
                ei_printf("Anomaly: %.3f\n", result.anomaly);
#endif
                ei_printf("----\n");
            }
            featureIx = 0;
        }
    }

    // ---------- EVENT RECORDING ----------
    if (now - lastRecordSampleTime < RECORD_SAMPLE_INTERVAL_MS) return;
    lastRecordSampleTime = now;

    auto frame = readSensors();
    float trigger = frame[0];

    if (!recording && trigger < START_LIMIT) {
        recording = true;
        sampleIndex = 0;
        Serial.println("[EVENT] START");
    }

    if (recording) {
        if (sampleIndex < MAX_SAMPLES) {
            samples[sampleIndex][0] = (float)millis();
            for (int i = 0; i < 8; i++) {
                samples[sampleIndex][i + 1] = frame[i];
            }
            sampleIndex++;
        }

        if (trigger > STOP_LIMIT) {
            recording = false;
            Serial.printf("[EVENT] STOP samples=%d\n", sampleIndex);
            uploadCSV();
            sampleIndex = 0;
        }
    }
}

// ================= INTERNAL =================

int ImpactDetection::eiGetData(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr,
           instancePtr->features + offset,
           length * sizeof(float));
    return 0;
}

std::array<float, 8> ImpactDetection::readSensors() {
    sensors_event_t t;
    mpu.getEvent(&accel, &gyro, &t);

    return {
        (float)analogRead(ANALOG_PIN),
        (float)digitalRead(DIGITAL_PIN),
        accel.acceleration.x,
        accel.acceleration.y,
        accel.acceleration.z,
        gyro.gyro.x,
        gyro.gyro.y,
        gyro.gyro.z
    };
}

void ImpactDetection::connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(wifiSsid, wifiPass);

    Serial.print("[WIFI] Connecting");
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WIFI] Connected");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WIFI] FAILED");
    }
}

void ImpactDetection::uploadCSV() {
    Serial.println("[HTTP] Uploading CSV");

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "text/csv");

    String body = "timestamp,analog,digital,ax,ay,az,gx,gy,gz\n";

    for (int i = 0; i < sampleIndex; i++) {
        for (int j = 0; j < 9; j++) {
            body += String(samples[i][j], 6);
            if (j < 8) body += ",";
        }
        body += "\n";
    }

    int code = http.POST(body);
    Serial.printf("[HTTP] Code: %d\n", code);

    http.end();
}
