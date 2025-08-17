#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// Returns system uptime in milliseconds, handling millis() overflow
inline uint64_t getUptimeMillis() {
  static uint32_t last = 0;
  static uint64_t total = 0;

  uint32_t now = millis();
  total += (uint32_t)(now - last);
  last = now;
  return total;
}

#endif
