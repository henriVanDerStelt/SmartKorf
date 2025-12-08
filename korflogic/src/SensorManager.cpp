#include "SensorManager.h"

SensorManager::SensorManager()
    : goalScored(false)
    , shotAttempt(false)
    , playerCollision(false)
    , topTriggerTime(0)
    , bottomTriggerTime(0)
    , lastVibrationTime(0)
    , topSensorTriggered(false)
    , bottomSensorTriggered(false)
    , lastVibrationLevel(0)
{
}

bool SensorManager::begin()
{
    Serial.println("Initializing Korfbal Detection System.");

// Initialiseer vibration sensor pins
#if USE_ANALOG_VIBRATION
    pinMode(VIBRATION_PIN_ANALOG, INPUT); // AO - analoge input
    analogReadResolution(12); // 12-bit ADC (0-4095)
    Serial.println("Vibration sensor: ANALOG mode (AO pin)");
#else
    pinMode(VIBRATION_PIN_DIGITAL, INPUT); // DO - digitale input
    Serial.println("Vibration sensor: DIGITAL mode (DO pin)");
#endif

    pinMode(TOF_XSHUT_TOP, OUTPUT);
    pinMode(TOF_XSHUT_BOT, OUTPUT);

    // I2C initialiseren
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000); // 400kHz Fast Mode

    delay(100);

    // Initialiseer sensoren
    if (!initializeTOF()) {
        Serial.println("TOF initialization failed!");
        return false;
    }

    if (!initializeMPU()) {
        Serial.println("MPU initialization failed!");
        return false;
    }

    // Configureer low power mode
    configureLowPowerMode();

    Serial.println("All sensors initialized successfully!");
    return true;
}

bool SensorManager::initializeTOF()
{
    // Reset beide TOF sensoren door XSHUT low te zetten
    digitalWrite(TOF_XSHUT_TOP, LOW);
    digitalWrite(TOF_XSHUT_BOT, LOW);
    delay(10);

    // Activeer alleen de top sensor
    digitalWrite(TOF_XSHUT_TOP, HIGH);
    digitalWrite(TOF_XSHUT_BOT, LOW);
    delay(10);

    // Initialiseer top sensor op standaard adres
    if (!tofTop.begin(TOF_TOP_ADDRESS, &Wire)) {
        Serial.println("Failed to initialize TOP TOF sensor!");
        return false;
    }

    // Verander het I2C adres van de top sensor
    if (!tofTop.VL53L1X_SetI2CAddress(TOF_TOP_ADDRESS << 1)) {
        Serial.println("Failed to set TOP TOF address!");
    }

    // Nu activeren we de bottom sensor
    digitalWrite(TOF_XSHUT_BOT, HIGH);
    delay(10);

    // Initialiseer bottom sensor (deze krijgt standaard 0x29)
    // Omdat de top sensor nu een ander adres heeft, is 0x29 vrij
    if (!tofBottom.begin(0x29, &Wire)) {
        Serial.println("Failed to initialize BOTTOM TOF sensor!");
        return false;
    }

    // Verander bottom sensor naar ons gewenste adres
    if (!tofBottom.VL53L1X_SetI2CAddress(TOF_BOTTOM_ADDRESS << 1)) {
        Serial.println("Failed to set BOTTOM TOF address!");
    }

    // Configureer beide sensoren voor snelle, energiezuinige metingen
    // Short distance mode, timing budget 20ms
    if (!tofTop.startRanging()) {
        Serial.println("Failed to start TOP TOF ranging!");
        return false;
    }
    tofTop.setTimingBudget(20);

    if (!tofBottom.startRanging()) {
        Serial.println("Failed to start BOTTOM TOF ranging!");
        return false;
    }
    tofBottom.setTimingBudget(20);

    Serial.println("TOF sensors initialized");
    return true;
}

bool SensorManager::initializeMPU()
{
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip!");
        return false;
    }

    // Configureer ranges
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // ±8g
    mpu.setGyroRange(MPU6050_RANGE_500_DEG); // ±500°/s
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Low-pass filter

    // Cycle mode voor energie besparing (wake up periodiek)
    mpu.setCycleRate(MPU6050_CYCLE_40_HZ); // 40Hz sample rate

    Serial.println("MPU6050 initialized");
    return true;
}

void SensorManager::configureLowPowerMode()
{
    // ESP32-C3 power management
    setCpuFrequencyMhz(80); // Verlaag CPU frequentie naar 80MHz

    // Configureer WiFi/BT uit (indien niet nodig)
    // btStop();
    // WiFi.mode(WIFI_OFF);

    Serial.println("Low power mode configured");
}

void SensorManager::update()
{
    checkTOFSensors();
    checkVibration();
    checkIMU();

    // Reset triggered sensors na timeout
    if (topSensorTriggered && (millis() - topTriggerTime > TOF_TIMEOUT_MS)) {
        topSensorTriggered = false;
    }
    if (bottomSensorTriggered && (millis() - bottomTriggerTime > TOF_TIMEOUT_MS)) {
        bottomSensorTriggered = false;
    }
}

void SensorManager::checkTOFSensors()
{
    // Check top sensor
    if (tofTop.dataReady()) {
        int16_t distanceTop = tofTop.distance();

        if (distanceTop > 0 && distanceTop < DETECTION_THRESHOLD_MM) {
            if (!topSensorTriggered) {
                topSensorTriggered = true;
                topTriggerTime = millis();
                Serial.printf("TOP sensor triggered: %d mm\n", distanceTop);

                // Check of bottom al getriggerd was (bal gaat door korf)
                if (bottomSensorTriggered && (topTriggerTime - bottomTriggerTime < TOF_TIMEOUT_MS)) {
                    goalScored = true;
                    Serial.println("⚡ GOAL SCORED! Ball passed through!");
                }
            }
        }
        tofTop.clearInterrupt();
    }

    // Check bottom sensor
    if (tofBottom.dataReady()) {
        int16_t distanceBottom = tofBottom.distance();

        if (distanceBottom > 0 && distanceBottom < DETECTION_THRESHOLD_MM) {
            if (!bottomSensorTriggered) {
                bottomSensorTriggered = true;
                bottomTriggerTime = millis();
                Serial.printf("BOTTOM sensor triggered: %d mm\n", distanceBottom);

                // Check of top al getriggerd was (bal gaat door korf)
                if (topSensorTriggered && (bottomTriggerTime - topTriggerTime < TOF_TIMEOUT_MS)) {
                    goalScored = true;
                    Serial.println("⚡ GOAL SCORED!  Ball passed through!");
                }
            }
        }
        tofBottom.clearInterrupt();
    }
}

void SensorManager::checkVibration()
{
    unsigned long currentTime = millis();
    bool vibrationDetected = false;

#if USE_ANALOG_VIBRATION
    // Lees analoge waarde (0-4095)
    uint16_t vibrationValue = analogRead(VIBRATION_PIN_ANALOG);
    lastVibrationLevel = vibrationValue;

    // Detecteer vibratie als waarde boven threshold komt
    if (vibrationValue > VIBRATION_ANALOG_THRESHOLD) {
        vibrationDetected = true;
    }
#else
    // Lees digitale waarde (HIGH/LOW)
    int vibrationState = digitalRead(VIBRATION_PIN_DIGITAL);
    lastVibrationLevel = (vibrationState == HIGH) ? 4095 : 0;

    if (vibrationState == VIBRATION_DIGITAL_THRESHOLD) {
        vibrationDetected = true;
    }
#endif

    // Debounce en verwerk vibratie detectie
    if (vibrationDetected && (currentTime - lastVibrationTime > VIBRATION_DEBOUNCE_MS)) {
        lastVibrationTime = currentTime;

#if USE_ANALOG_VIBRATION
        Serial.printf("⚠️ Vibration detected! Level: %d/4095\n", lastVibrationLevel);
#else
        Serial.println("⚠️ Vibration detected!");
#endif

        // Als er geen TOF detectie is, is het waarschijnlijk een speler collision
        if (!topSensorTriggered && !bottomSensorTriggered) {
            playerCollision = true;
            Serial.println("👤 Player collision detected!");
        }
    }
}

void SensorManager::checkIMU()
{
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    // Bereken magnitude van acceleratie en gyro
    float accelMagnitude = sqrt(accel.acceleration.x * accel.acceleration.x
        + accel.acceleration.y * accel.acceleration.y + accel.acceleration.z * accel.acceleration.z);

    float gyroMagnitude = sqrt(gyro.gyro.x * gyro.gyro.x + gyro.gyro.y * gyro.gyro.y + gyro.gyro.z * gyro.gyro.z);

    // Detecteer significante beweging
    if (accelMagnitude > ACCEL_THRESHOLD || gyroMagnitude > GYRO_THRESHOLD) {
        if (detectBallVsPlayerImpact(accelMagnitude, gyroMagnitude, lastVibrationLevel)) {
            if (!shotAttempt) {
                shotAttempt = true;
                Serial.println("🏀 Shot attempt detected!");
            }
        }
    }
}

bool SensorManager::detectBallVsPlayerImpact(float accelMagnitude, float gyroMagnitude, uint16_t vibrationLevel)
{
    // Bal impact: hoge versnelling, lage rotatie, korte hoge vibratie piek
    // Speler impact: matige versnelling, hogere rotatie, langere vibratie

    float impactRatio = accelMagnitude / (gyroMagnitude + 0.1); // Voorkom delen door 0

    // Normaliseer vibratie level (0.0 - 1.0)
    float vibrationIntensity = vibrationLevel / 4095.0;

    // Bal impact kenmerken:
    // - Hoge impact ratio (snelle versnelling zonder rotatie)
    // - Hoge vibratie intensiteit (korte piek)
    if (impactRatio > 3.0 && accelMagnitude > ACCEL_THRESHOLD && vibrationIntensity > 0.3) {
        return true; // Bal impact
    }

    // Speler collision kenmerken:
    // - Veel rotatie (speler duwt/leunt tegen korf)
    // - Lagere vibratie (meer gedempte kracht)
    if (gyroMagnitude > GYRO_THRESHOLD * 2 || vibrationIntensity < 0.2) {
        playerCollision = true;
        return false;
    }

    return false;
}

void SensorManager::resetDetections()
{
    goalScored = false;
    shotAttempt = false;
    playerCollision = false;
    topSensorTriggered = false;
    bottomSensorTriggered = false;
    topTriggerTime = 0;
    bottomTriggerTime = 0;
    lastVibrationLevel = 0;
}

void SensorManager::printStatus()
{
    Serial.println("=== Sensor Status ===");
    Serial.printf("Goal Scored: %s\n", goalScored ? "YES" : "NO");
    Serial.printf("Shot Attempt: %s\n", shotAttempt ? "YES" : "NO");
    Serial.printf("Player Collision: %s\n", playerCollision ? "YES" : "NO");
    Serial.printf("Top Sensor: %s\n", topSensorTriggered ? "TRIGGERED" : "IDLE");
    Serial.printf("Bottom Sensor: %s\n", bottomSensorTriggered ? "TRIGGERED" : "IDLE");
    Serial.printf("Vibration Level: %d/4095\n", lastVibrationLevel);
    Serial.println("====================");
}

void SensorManager::enterDeepSleep(uint64_t sleepTimeSeconds)
{
    Serial.printf("Entering deep sleep for %llu seconds...\n", sleepTimeSeconds);

    // Schakel sensoren uit om stroom te besparen
    digitalWrite(TOF_XSHUT_TOP, LOW);
    digitalWrite(TOF_XSHUT_BOT, LOW);

    // Configureer wake-up sources
    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);

    // ESP32-C3 gebruikt GPIO wakeup in plaats van ext0/ext1
    // Wake up op HIGH niveau op vibration pin
    esp_sleep_enable_gpio_wakeup();

#if USE_ANALOG_VIBRATION
    gpio_wakeup_enable((gpio_num_t)VIBRATION_PIN_ANALOG, GPIO_INTR_HIGH_LEVEL);
#else
    gpio_wakeup_enable((gpio_num_t)VIBRATION_PIN_DIGITAL, GPIO_INTR_HIGH_LEVEL);
#endif

    esp_deep_sleep_start();
}