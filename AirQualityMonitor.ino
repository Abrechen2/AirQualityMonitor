// ===== AIR QUALITY MONITOR V1.5.1 - INTERNAL CALCULATIONS =====
// Advanced version with BSEC LP Mode, Stealth Control, CO2/VOC, Internal Calculations
// Author: Dennis Wittke
// Version: 1.5.1 - Bugfixes and stability improvements
// Date: 2025

#include <Arduino.h>
#include "bsec.h"
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include "PMS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

// Project includes
#include "config.h"
#include "secrets.h"
#include "ConfigManager.h"
#include "SensorManager.h"
#include "DisplayManager.h"
#include "ButtonHandler.h"
#include "LEDManager.h"
#include "WiFiManager.h"
#include "Calculations.h"
#include "MQTTManager.h"

// ===== HARDWARE OBJECTS =====
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Bsec iaqSensor;
PMS pms(Serial1);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, DISPLAY_SCL, DISPLAY_SDA);

// ===== SYSTEM OBJECTS =====
ConfigManager configManager;
SensorManager sensorManager(iaqSensor, pms);
DisplayManager displayManager(u8g2, strip);
ButtonHandler buttonHandler(displayManager);
LEDManager ledManager(strip, displayManager);
WiFiManager wifiManager(&configManager);
MQTTManager mqttManager(&configManager);

// ===== GLOBAL VARIABLES =====
bool wifiConnected = false;
float calculatedAQI = 50.0;
const char* aqiLevel = "Good";
uint32_t aqiColorCode = 0x00FF00; // Green

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DEBUG_INFO("=== Air Quality Monitor starting ===");
  
  // Print ESP32 Core version information
  #if defined(ESP32)
    DEBUG_INFO("ESP32 Arduino Core Version: %s", ESP.getSdkVersion());
    DEBUG_INFO("ESP32 Chip Model: %s", ESP.getChipModel());
    DEBUG_INFO("ESP32 Chip Revision: %d", ESP.getChipRevision());
    DEBUG_INFO("ESP32 CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
  #endif
 
  // Initialize hardware
  Wire.begin(DISPLAY_SDA, DISPLAY_SCL);
  Wire.setClock(100000);
  delay(100);
  
  Serial1.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);
  delay(100);

  // Initialize components
  displayManager.init();
  ledManager.init();
  buttonHandler.init();
  
  // Show startup messages
  displayManager.showMessage("System starting...");

  // Initialize sensors (also initializes EEPROM)
  displayManager.showMessage("Initializing sensors...");
  bool sensorsOK = sensorManager.init();
  
  if (sensorsOK) {
    displayManager.showMessage("Sensors OK!", 1000);
  } else {
    displayManager.showMessage("Sensor warning!", 5000);
  }

  // Initialize config manager (after EEPROM is initialized)
  if (!configManager.init()) {
    DEBUG_ERROR("ConfigManager initialization failed");
  }

  // Connect to WiFi
  displayManager.showMessage("Connecting WiFi...");
  wifiConnected = wifiManager.connect();

  if (wifiConnected) {
    displayManager.showMessage("WiFi connected!", 1000);
    String ip = wifiManager.getIPAddress();
    displayManager.showMessage("IP: " + ip, 2000);
    DEBUG_INFO("WiFi connected successfully");

    // Initialize MQTT
    mqttManager.init();
    if (mqttManager.connect()) {
      DEBUG_INFO("MQTT connected successfully");
    } else {
      DEBUG_WARN("MQTT connection failed - will retry in loop");
    }
    
    // Initialize Arduino OTA
    initOTA();
  } else {
    displayManager.showMessage("Offline mode", 5000);
    DEBUG_WARN("WiFi connection failed - offline mode");
  }

  displayManager.showMessage("System ready!", 1000);
  DEBUG_INFO("Setup completed");
}

void loop() {
  static unsigned long lastDebugTime = 0;
  static uint8_t loopDebugCount = 0;

  // Update system components
  buttonHandler.update();

  // Read sensors
  if (sensorManager.update()) {
    SensorData data = sensorManager.getData();

    // Enhanced debug output for sensor data
    if (loopDebugCount < 10 || (millis() - lastDebugTime > 30000)) {
      DEBUG_INFO("=== Sensor Data Update ===");
      if (data.bme68xAvailable) {
        DEBUG_INFO("BME68X - Temp: %.1f°C, Hum: %.1f%%, Press: %.1f hPa, Gas: %.0f Ohm",
                   data.temperature, data.humidity, data.pressure, data.gasResistance);
        DEBUG_INFO("BSEC - IAQ: %.0f (acc:%d), CO2: %.0f ppm (acc:%d), VOC: %.1f mg/m3 (acc:%d)",
                   data.iaq, data.iaqAccuracy,
                   data.co2Equivalent, data.co2Accuracy,
                   data.breathVocEquivalent, data.breathVocAccuracy);
      } else {
        DEBUG_WARN("BME68X not available!");
      }
      if (data.pms5003Available) {
        DEBUG_INFO("PMS5003 - PM1.0: %d, PM2.5: %d, PM10: %d µg/m³",
                   data.pm1_0, data.pm2_5, data.pm10);
      }
      lastDebugTime = millis();
      loopDebugCount++;
    }

    // Calculate AQI internally using Calculations.h
    calculatedAQI = calculateCombinedAQI(data);
    aqiLevel = getAQILevelString(calculatedAQI);
    aqiColorCode = calculateAQIColor(calculatedAQI);

    // Update display and LEDs
    displayManager.updateDisplay(data, calculatedAQI, String(aqiLevel), wifiConnected);
    ledManager.updateLEDsWithTransition(aqiColorCode);
    
    // Publish data to MQTT with all calculated values
    if (wifiConnected) {
      mqttManager.update();
      mqttManager.publishData(data, calculatedAQI, String(aqiLevel), wifiConnected, displayManager);
    }
  }

  // Update MQTT connection (even when no sensor data update)
  if (wifiConnected) {
    mqttManager.update();
    
    // Handle Arduino OTA updates
    ArduinoOTA.handle();
  }

  // Check WiFi connection - always try to reconnect if not connected
  if (!wifiManager.isConnected()) {
    static unsigned long lastWifiReconnect = 0;
    unsigned long now = millis();
    
    // Try to reconnect every 30 seconds
    if (now - lastWifiReconnect >= 30000) {
      lastWifiReconnect = now;
      DEBUG_WARN("WiFi not connected - attempting reconnection");
      bool newConnection = wifiManager.connect();
      
      if (newConnection) {
        wifiConnected = true;
        DEBUG_INFO("WiFi reconnected successfully");
        // Reinitialize MQTT and OTA after WiFi reconnection
        mqttManager.init();
        if (mqttManager.connect()) {
          DEBUG_INFO("MQTT reconnected after WiFi reconnection");
        }
        initOTA();
      } else {
        wifiConnected = false;
        DEBUG_WARN("WiFi reconnection failed - will retry in 30 seconds");
      }
    }
  } else if (!wifiConnected) {
    // WiFi has reconnected automatically (via setAutoReconnect)
    wifiConnected = true;
    DEBUG_INFO("WiFi auto-reconnected - reinitializing MQTT and OTA");
    mqttManager.init();
    if (mqttManager.connect()) {
      DEBUG_INFO("MQTT reconnected after WiFi auto-reconnection");
    }
    initOTA();
  }
  
  // Always update LED transitions (even when no sensor update)
  ledManager.updateLEDsWithTransition(aqiColorCode);
}

// ===== ARDUINO OTA FUNCTIONS =====
void initOTA() {
  DEBUG_INFO("Initializing Arduino OTA...");
  
  // Set hostname - use configured hostname if available, otherwise fallback to MAC-based
  String hostname;
  if (strlen(configManager.getHostname()) > 0) {
    hostname = String(configManager.getHostname());
  } else {
    hostname = "AirQualityMonitor_" + WiFi.macAddress();
    hostname.replace(":", "");
  }
  ArduinoOTA.setHostname(hostname.c_str());
  
  // Set password if configured
  #ifdef OTA_PASSWORD
    ArduinoOTA.setPassword(OTA_PASSWORD);
    DEBUG_INFO("OTA password protection enabled");
  #else
    DEBUG_WARN("OTA password not set - updates are unprotected!");
  #endif
  
  // OTA callbacks
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    DEBUG_INFO("Start updating %s", type.c_str());
  });
  
  ArduinoOTA.onEnd([]() {
    DEBUG_INFO("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_INFO("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_ERROR("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_ERROR("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      DEBUG_ERROR("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      DEBUG_ERROR("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_ERROR("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      DEBUG_ERROR("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  DEBUG_INFO("Arduino OTA ready - Hostname: %s", hostname.c_str());
}