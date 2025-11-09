// ===== AIR QUALITY MONITOR V6 - STEALTH & BSEC OPTIMIZED =====
// Advanced version with BSEC ULP Mode, Stealth Control, CO2/VOC, Byte Transmission
// Author: Dennis Wittke  
// Version: 6.0 - Complete Stealth & Gas Sensor Integration + Byte Transmission
// Date: 2025

#include <Arduino.h>
#include "bsec.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include "PMS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Project includes
#include "config.h"
#include "secrets.h"
#include "SensorManager.h"
#include "DisplayManager.h"
#include "ButtonHandler.h"
#include "LEDManager.h"
#include "ByteTransmission.h"

// ===== HARDWARE OBJECTS =====
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Bsec iaqSensor;
PMS pms(Serial1);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, DISPLAY_SCL, DISPLAY_SDA);

// ===== SYSTEM OBJECTS =====
SensorManager sensorManager(iaqSensor, pms);
DisplayManager displayManager(u8g2, strip);
ButtonHandler buttonHandler(displayManager);
LEDManager ledManager(strip, displayManager);
ByteTransmissionManager byteManager;

// ===== GLOBAL VARIABLES =====
bool wifiConnected = false;
bool nodeRedResponding = false;  // Node-RED response status
float calculatedAQI = 50.0;
String aqiLevel = "Good";
uint32_t aqiColorCode = 0x00FF00; // Green

AQIResult calculateLocalAQI(const SensorData& data) {
  AQIResult result;

  // Validate input PM2.5 value
  float pm25 = (float)data.pm2_5;
  if (pm25 < 0) {
    DEBUG_WARN("Invalid PM2.5 value: %.1f", pm25);
    pm25 = 0;
  }

  // Clamp to maximum valid value
  if (pm25 > PM_MAX_VALID) {
    DEBUG_WARN("PM2.5 exceeds max valid: %.1f", pm25);
    pm25 = PM_MAX_VALID;
  }

  float aqi = 0;

  // US EPA PM2.5 AQI calculation using named constants
  if (pm25 <= AQI_PM25_GOOD_MAX) {
    // Good (0-50)
    aqi = pm25 * 50.0 / AQI_PM25_GOOD_MAX;
    result.level = F("Good");
    result.colorCode = 0x00FF00;
  } else if (pm25 <= AQI_PM25_MODERATE_MAX) {
    // Moderate (51-100)
    aqi = (pm25 - AQI_PM25_GOOD_MAX) * (100.0 - 51.0) /
          (AQI_PM25_MODERATE_MAX - AQI_PM25_GOOD_MAX) + 51.0;
    result.level = F("Moderate");
    result.colorCode = 0xFFFF00;
  } else if (pm25 <= AQI_PM25_UNHEALTHY_SENSITIVE_MAX) {
    // Unhealthy for Sensitive Groups (101-150)
    aqi = (pm25 - AQI_PM25_MODERATE_MAX) * (150.0 - 101.0) /
          (AQI_PM25_UNHEALTHY_SENSITIVE_MAX - AQI_PM25_MODERATE_MAX) + 101.0;
    result.level = F("Poor");
    result.colorCode = 0xFFA500;
  } else if (pm25 <= AQI_PM25_UNHEALTHY_MAX) {
    // Unhealthy (151-200)
    aqi = (pm25 - AQI_PM25_UNHEALTHY_SENSITIVE_MAX) * (200.0 - 151.0) /
          (AQI_PM25_UNHEALTHY_MAX - AQI_PM25_UNHEALTHY_SENSITIVE_MAX) + 151.0;
    result.level = F("Unhealthy");
    result.colorCode = 0xFF0000;
  } else if (pm25 <= AQI_PM25_VERY_UNHEALTHY_MAX) {
    // Very Unhealthy (201-300)
    aqi = (pm25 - AQI_PM25_UNHEALTHY_MAX) * (300.0 - 201.0) /
          (AQI_PM25_VERY_UNHEALTHY_MAX - AQI_PM25_UNHEALTHY_MAX) + 201.0;
    result.level = F("Very poor");
    result.colorCode = 0x800080;
  } else {
    // Hazardous (301-500)
    aqi = (pm25 - AQI_PM25_VERY_UNHEALTHY_MAX) * (500.0 - 301.0) /
          (AQI_PM25_HAZARDOUS_MAX - AQI_PM25_VERY_UNHEALTHY_MAX) + 301.0;
    // Cap at 500
    aqi = min(aqi, 500.0f);
    result.level = F("Hazardous");
    result.colorCode = 0x7E0023;
  }

  result.aqi = aqi;
  result.success = true;
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DEBUG_INFO("=== Air Quality Monitor starting ===");
 
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

  // Initialize sensors
  displayManager.showMessage("Initializing sensors...");
  bool sensorsOK = sensorManager.init();
  
  if (sensorsOK) {
    displayManager.showMessage("Sensors OK!", 1000);
  } else {
    displayManager.showMessage("Sensor warning!", 5000);
  }

  // Connect to WiFi
  displayManager.showMessage("Connecting WiFi...");
  wifiConnected = byteManager.connectWiFi();

  if (wifiConnected) {
    displayManager.showMessage("WiFi connected!", 1000);
    String ip = WiFi.localIP().toString();
    displayManager.showMessage("IP: " + ip, 2000);
    DEBUG_INFO("WiFi connected successfully");
  } else {
    displayManager.showMessage("Offline mode", 5000);
    DEBUG_WARN("WiFi connection failed - offline mode");
  }

  displayManager.showMessage("System ready!", 1000);
  DEBUG_INFO("Setup completed");
}

void loop() {
  // Update system components
  buttonHandler.update();
  
  // Read sensors
  if (sensorManager.update()) {
    SensorData data = sensorManager.getData();


    AQIResult local = calculateLocalAQI(data);

    if (wifiConnected) {
      if (byteManager.isTimeToSend()) {
        AQIResult net = byteManager.sendDataAndGetAQI(data);
        if (net.success) {
          calculatedAQI = net.aqi;
          aqiLevel = net.level;
          aqiColorCode = net.colorCode;
          nodeRedResponding = true;
          DEBUG_INFO("Received AQI from Node-RED: %.1f (%s)", calculatedAQI, aqiLevel.c_str());
        } else {
          nodeRedResponding = false;
          calculatedAQI = local.aqi;
          aqiLevel = local.level;
          aqiColorCode = local.colorCode;
          DEBUG_WARN("Node-RED timeout or error");
        }
      } else if (!nodeRedResponding) {
        calculatedAQI = local.aqi;
        aqiLevel = local.level;
        aqiColorCode = local.colorCode;
      }
    } else {
      nodeRedResponding = false;
      calculatedAQI = local.aqi;
      aqiLevel = local.level;
      aqiColorCode = local.colorCode;
    }

    displayManager.updateDisplay(data, calculatedAQI, aqiLevel, wifiConnected, nodeRedResponding);
    ledManager.updateLEDs(aqiColorCode);

    // Debug output for BSEC values
    if (data.bme68xAvailable) {
      DEBUG_INFO("BSEC - CO2: %.0f ppm (acc:%d), VOC: %.1f mg/m3 (acc:%d), IAQ: %.0f (acc:%d)",
                 data.co2Equivalent, data.co2Accuracy,
                 data.breathVocEquivalent, data.breathVocAccuracy,
                 data.iaq, data.iaqAccuracy);
    }
  }
  
  // Check WiFi connection with exponential backoff
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    DEBUG_WARN("WiFi connection lost");
    wifiConnected = false;
  }

  // Attempt reconnection if not connected (with exponential backoff)
  if (!wifiConnected && byteManager.canAttemptWiFiReconnect()) {
    wifiConnected = byteManager.reconnectWiFi();
  }

  delay(100);
}