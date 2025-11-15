#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// Returns system uptime in milliseconds, handling millis() overflow
inline uint64_t getUptimeMillis() {
  static uint32_t last = 0;
  static uint64_t total = 0;
  static bool initialized = false;

  uint32_t now = millis();

  // First call initialization - prevent huge initial delta
  if (!initialized) {
    last = now;
    initialized = true;
    return 0;
  }

  total += (uint32_t)(now - last);
  last = now;
  return total;
}

#endif
