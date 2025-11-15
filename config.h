#ifndef CONFIG_H
#define CONFIG_H

// ===== HARDWARE CONFIGURATION =====
#define LED_PIN 5
#define NUM_LEDS 3
#define LED_BRIGHTNESS_NORMAL 20
#define LED_BRIGHTNESS_STEALTH 1

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DISPLAY_SCL 22
#define DISPLAY_SDA 21
#define DISPLAY_CONTRAST_NORMAL 255
#define DISPLAY_CONTRAST_STEALTH 0

// Sensors
#define PMS_RX_PIN 16
#define PMS_TX_PIN 17
#define DS18B20_PIN 27  // GPIO27 (original configuration retained)

// Button - only use select
#define BUTTON_SELECT_PIN 33
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 2000

// ===== TIMING CONFIGURATION =====
#define DATA_SEND_INTERVAL 10000      // 10 seconds
#define SENSOR_READ_INTERVAL 3000     // 3 seconds (BSEC ULP mode compromise)
#define WIFI_CONNECT_TIMEOUT 15000    // 15 seconds
#define STEALTH_TEMP_ON_MS 20000      // 20 seconds temporary activation

// ===== SENSOR CONFIGURATION =====
#define DEFAULT_TEMP_CORRECTION -3.5
#define DEFAULT_HUMIDITY_CORRECTION 0.0

// BSEC configuration
#define BSEC_STATE_SAVE_INTERVAL 21600000  // 6 hours in ms
#define BSEC_BASELINE_EEPROM_ADDR 0

// ===== DISPLAY VIEWS =====
enum DisplayView {
  VIEW_OVERVIEW = 0,
  VIEW_ENVIRONMENT,
  VIEW_PARTICLES,
  VIEW_GAS,      // Gas sensors
  VIEW_SYSTEM,
  VIEW_COUNT
};

// ===== STEALTH MODE =====
enum StealthMode {
  STEALTH_OFF = 0,
  STEALTH_ON,
  STEALTH_TEMP_ON  // Temporarily enabled
};

// ===== WIFI MANAGER CONFIGURATION =====
// Uncomment to use WiFiManager library for credential management
// This provides a web portal for WiFi configuration (more secure than hardcoded credentials)
// Requires: WiFiManager library (https://github.com/tzapu/WiFiManager)
// #define USE_WIFI_MANAGER 1

// ===== DEBUG CONFIGURATION =====
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
  // Basic debug macros
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
  // Structured logging helpers for clearer output
  #define DEBUG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
  #define DEBUG_WARN(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
  #define DEBUG_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
  #define DEBUG_INFO(fmt, ...)
  #define DEBUG_WARN(fmt, ...)
  #define DEBUG_ERROR(fmt, ...)
#endif

#endif