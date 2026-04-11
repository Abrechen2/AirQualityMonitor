#ifndef CONFIG_H
#define CONFIG_H

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "1.5.5"

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

// BME680/BME688 I2C addresses (standard: 0x76 with SDO low, 0x77 with SDO high)
#ifndef BME68X_I2C_ADDR_LOW
#define BME68X_I2C_ADDR_LOW 0x76
#endif
#ifndef BME68X_I2C_ADDR_HIGH
#define BME68X_I2C_ADDR_HIGH 0x77
#endif

// Button - only use select
#define BUTTON_SELECT_PIN 33
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 2000

// ===== TIMING CONFIGURATION =====
#define DATA_SEND_INTERVAL 10000       // 10 seconds
#define SENSOR_READ_INTERVAL 3000      // 3 seconds (BSEC ULP mode compromise)
#define WIFI_CONNECT_TIMEOUT 8000      // 8 seconds
#define STEALTH_TEMP_ON_MS 20000       // 20 seconds temporary activation
#define LED_TRANSITION_STEPS 20        // Number of steps for smooth color transition
#define LED_TRANSITION_INTERVAL_MS 50  // Time between transition steps (ms)

// ===== MQTT CONFIGURATION =====
#define MQTT_PUBLISH_INTERVAL 10000           // 10 seconds (synchronized with DATA_SEND_INTERVAL)
#define MQTT_RECONNECT_INTERVAL 5000          // 5 seconds between reconnect attempts
#define MQTT_MAX_PACKET_SIZE 2048             // Maximum MQTT packet size (bytes)
#define DISCOVERY_REPUBLISH_INTERVAL 3600000  // 60 minutes (ms) - republish discovery config periodically

// ===== SENSOR CONFIGURATION =====
#define DEFAULT_TEMP_CORRECTION -3.5
#define DEFAULT_HUMIDITY_CORRECTION 0.0

// BSEC configuration
#define BSEC_STATE_SAVE_INTERVAL 21600000  // 6 hours in ms
#define BSEC_BASELINE_EEPROM_ADDR 0

// Config storage in EEPROM (after BSEC state)
#define CONFIG_EEPROM_ADDR 512  // Start address for config storage
#define CONFIG_EEPROM_SIZE 256  // Size allocated for config (max 512 total)

// ===== VALIDATION CONSTANTS =====
// Sensor value ranges for plausibility checks
#define TEMP_MIN -40.0f       // Minimum valid temperature (°C)
#define TEMP_MAX 85.0f        // Maximum valid temperature (°C)
#define HUMIDITY_MIN 0.0f     // Minimum valid humidity (%)
#define HUMIDITY_MAX 100.0f   // Maximum valid humidity (%)
#define PRESSURE_MIN 300.0f   // Minimum valid pressure (hPa)
#define PRESSURE_MAX 1100.0f  // Maximum valid pressure (hPa)
#define IAQ_MIN 0.0f          // Minimum valid IAQ
#define IAQ_MAX 500.0f        // Maximum valid IAQ
#define CO2_MIN 400.0f        // Minimum valid CO2 (ppm)
#define CO2_MAX 40000.0f      // Maximum valid CO2 (ppm)
#define VOC_MIN 0.0f          // Minimum valid VOC (mg/m³)
#define VOC_MAX 60.0f         // Maximum valid VOC (mg/m³)
#define PM_MIN 0              // Minimum valid PM value (µg/m³)
#define PM_MAX 1000           // Maximum valid PM value (µg/m³)

// HTTP/Network constants
#define HTTP_MAX_RESPONSE_SIZE 1024  // Maximum HTTP response size (bytes)
#define HTTP_TIMEOUT_MS 5000         // HTTP request timeout (ms)
#define HTTP_AQI_TIMEOUT_MS 3000     // AQI request timeout (ms)
#define JSON_MAX_SIZE 256            // Maximum JSON document size (bytes)

// PMS5003 constants
#define PMS_READ_TIMEOUT_MS 1000  // PMS5003 read timeout (ms)
#define PMS_WAKE_TIME_MS 2000     // PMS5003 wake time (ms)
#define PMS_RETRY_COUNT 2         // Maximum retry attempts for PMS5003

// DS18B20 constants
#define DS18B20_CONVERSION_TIME_MS 750  // DS18B20 conversion time (ms)
#define DS18B20_READ_INTERVAL_MS 10000  // DS18B20 read interval (ms)

// Type conversion limits (for overflow protection)
#define INT16_TEMP_MAX 3276        // Max temp * 100 for int16_t (32.76°C)
#define INT16_TEMP_MIN -3276       // Min temp * 100 for int16_t (-32.76°C)
#define UINT16_HUMIDITY_MAX 10000  // Max humidity * 100 for uint16_t (100.00%)
#define UINT16_PRESSURE_MAX 6553   // Max pressure * 10 for uint16_t (655.3 hPa)
#define UINT16_IAQ_MAX 5000        // Max IAQ * 10 for uint16_t (500.0)
#define UINT16_VOC_MAX 6000        // Max VOC * 100 for uint16_t (60.00 mg/m³)

// ===== DISPLAY VIEWS =====
enum DisplayView {
  VIEW_OVERVIEW = 0,
  VIEW_ENVIRONMENT,
  VIEW_PARTICLES,
  VIEW_GAS,  // Gas sensors
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

// ===== ERROR CODES =====
enum ErrorCode {
  ERROR_NONE = 0,
  ERROR_SENSOR_INIT_FAILED = 1,
  ERROR_SENSOR_READ_FAILED = 2,
  ERROR_WIFI_CONNECT_FAILED = 3,
  ERROR_MQTT_CONNECT_FAILED = 4,
  ERROR_HTTP_REQUEST_FAILED = 5,
  ERROR_JSON_PARSE_FAILED = 6,
  ERROR_EEPROM_FAILED = 7,
  ERROR_INVALID_DATA = 8,
  ERROR_OVERFLOW = 9
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