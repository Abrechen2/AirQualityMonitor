#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// Returns system uptime in milliseconds, handling millis() overflow
// Thread-safe implementation using atomic operations
inline uint64_t getUptimeMillis() {
  static uint32_t last = 0;
  static uint64_t total = 0;
  static bool initialized = false;
  static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  // Use critical section for thread safety (not ISR-safe, but safe for multi-core)
  portENTER_CRITICAL(&mux);
  uint32_t now = millis();

  // First call initialization - prevent huge initial delta
  if (!initialized) {
    last = now;
    initialized = true;
    portEXIT_CRITICAL(&mux);
    return 0;
  }

  // Handle millis() overflow (happens every ~49 days)
  uint32_t delta;
  if (now >= last) {
    delta = now - last;
  } else {
    // Overflow occurred
    delta = (UINT32_MAX - last) + now + 1;
  }

  total += delta;
  last = now;
  portEXIT_CRITICAL(&mux);
  return total;
}

#endif
