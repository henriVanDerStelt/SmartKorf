#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include <Wire.h>

// ---------------- CONFIG ----------------
#ifndef PCF8574_ADDR
#define PCF8574_ADDR 0x20
#endif

static constexpr uint8_t kButtonCount = 7;
static constexpr uint8_t kMask        = 0b01111111; // P0..P6

#ifndef PCF_POLL_MS
#define PCF_POLL_MS 5
#endif

#ifndef PCF_DEBOUNCE_MS
#define PCF_DEBOUNCE_MS 25
#endif

// ---------------- STATE ----------------
static uint8_t  g_stable           = 0x00; // active-high: 1=pressed
static uint8_t  g_rawLast          = 0x00;
static uint8_t  g_changed          = 0x00;
static uint8_t  g_candidate        = 0x00;
static uint32_t g_candidateSinceMs = 0;
static uint32_t g_lastPollMs       = 0;

// ---------------- I2C helpers ----------------
static inline bool pcf_write(uint8_t value) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

static inline bool pcf_read(uint8_t &valueOut) {
  uint8_t n = Wire.requestFrom((uint8_t)PCF8574_ADDR, (uint8_t)1);
  if (n != 1) return false;
  valueOut = Wire.read();
  return true;
}

// ---------------- PUBLIC API ----------------

// Call once in setup()
inline bool buttons_begin() {
  Wire.begin();

  // PCF8574 quasi-bidirectional: write 1s so pins act as inputs
  if (!pcf_write(0xFF)) return false;

  uint8_t v = 0;
  if (!pcf_read(v)) return false;

  v &= kMask;
  g_rawLast = v;
  g_stable = v;
  g_candidate = v;
  g_candidateSinceMs = millis();
  g_lastPollMs = millis();
  g_changed = 0;
  return true;
}

// Call often in loop(); it self-throttles by PCF_POLL_MS
inline void buttons_task() {
  const uint32_t now = millis();
  if (now - g_lastPollMs < PCF_POLL_MS) return;
  g_lastPollMs = now;

  uint8_t raw = 0;
  if (!pcf_read(raw)) return;
  raw &= kMask;

  // Debounce with candidate state
  if (raw != g_candidate) {
    g_candidate = raw;
    g_candidateSinceMs = now;
    return;
  }

  if ((now - g_candidateSinceMs) >= PCF_DEBOUNCE_MS) {
    const uint8_t diff = (g_stable ^ g_candidate) & kMask;
    if (diff) {
      g_stable = g_candidate;
      g_changed |= diff;
    }
  }

  g_rawLast = raw;
}

// Returns bitmask of changed buttons since last call (bit i => changed)
inline uint8_t buttons_consumeChanged() {
  uint8_t c = g_changed & kMask;
  g_changed = 0;
  return c;
}

// Active-high: true = pressed
inline bool button_isPressed(uint8_t index) {
  if (index >= kButtonCount) return false;
  return (g_stable >> index) & 0x01;
}

#endif // BUTTONS_H
