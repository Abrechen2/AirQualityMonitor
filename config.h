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

// ===== NETWORK CONFIGURATION =====
#define HTTP_MAX_RETRIES 3            // Maximum HTTP request retries
#define HTTP_RETRY_DELAY_MS 2000      // Initial retry delay (exponential backoff)
#define WIFI_RECONNECT_INTERVAL 60000 // 60 seconds between WiFi reconnection attempts
#define HTTP_RESPONSE_MAX_SIZE 2048   // Maximum expected HTTP response size

// ===== SENSOR CONFIGURATION =====
#define DEFAULT_TEMP_CORRECTION -3.5
#define DEFAULT_HUMIDITY_CORRECTION 0.0

// Sensor value validation ranges
#define TEMP_MIN_VALID -40.0      // Minimum valid temperature (°C)
#define TEMP_MAX_VALID 85.0       // Maximum valid temperature (°C)
#define HUMIDITY_MIN_VALID 0.0    // Minimum valid humidity (%)
#define HUMIDITY_MAX_VALID 100.0  // Maximum valid humidity (%)
#define PRESSURE_MIN_VALID 300.0  // Minimum valid pressure (hPa)
#define PRESSURE_MAX_VALID 1100.0 // Maximum valid pressure (hPa)
#define PM_MAX_VALID 1000         // Maximum valid PM value (µg/m³)
#define IAQ_MAX_VALID 500         // Maximum valid IAQ value
#define CO2_MIN_VALID 400         // Minimum valid CO2 (ppm)
#define CO2_MAX_VALID 10000       // Maximum valid CO2 (ppm)

// AQI Calculation Thresholds (US EPA PM2.5 breakpoints)
#define AQI_PM25_GOOD_MAX 12.0
#define AQI_PM25_MODERATE_MAX 35.4
#define AQI_PM25_UNHEALTHY_SENSITIVE_MAX 55.4
#define AQI_PM25_UNHEALTHY_MAX 150.4
#define AQI_PM25_VERY_UNHEALTHY_MAX 250.4
#define AQI_PM25_HAZARDOUS_MAX 500.4

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