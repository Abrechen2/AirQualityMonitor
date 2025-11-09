#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// Returns system uptime in milliseconds, handling millis() overflow
inline uint64_t getUptimeMillis() {
  static uint32_t last = 0;
  static uint64_t total = 0;
  static bool initialized = false;

  uint32_t now = millis();

  // Initialize on first call
  if (!initialized) {
    last = now;
    initialized = true;
    return total;
  }

  // Handle overflow: now < last means millis() wrapped around
  if (now >= last) {
    total += (now - last);
  } else {
    // Overflow occurred: calculate difference across the wrap
    total += ((0xFFFFFFFF - last) + now + 1);
  }

  last = now;
  return total;
}

#endif
