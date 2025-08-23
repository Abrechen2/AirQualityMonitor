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
bool nodeRedResponding = false;  // Node-RED Response Status
float calculatedAQI = 50.0;
String aqiLevel = "Gut";
uint32_t aqiColorCode = 0x00FF00; // Green

AQIResult calculateLocalAQI(const SensorData& data) {
  AQIResult result;
  int pm25 = data.pm2_5;
  float aqi = 0;

  if (pm25 <= 12) {
    aqi = pm25 * 50.0 / 12.0;
    result.level = F("Gut");
    result.colorCode = 0x00FF00;
  } else if (pm25 <= 35) {
    aqi = (pm25 - 12.1) * (100.0 - 51.0) / (35.4 - 12.1) + 51.0;
    result.level = F("Mittel");
    result.colorCode = 0xFFFF00;
  } else if (pm25 <= 55) {
    aqi = (pm25 - 35.5) * (150.0 - 101.0) / (55.4 - 35.5) + 101.0;
    result.level = F("Schlecht");
    result.colorCode = 0xFFA500;
  } else if (pm25 <= 150) {
    aqi = (pm25 - 55.5) * (200.0 - 151.0) / (150.4 - 55.5) + 151.0;
    result.level = F("Ungesund");
    result.colorCode = 0xFF0000;
  } else if (pm25 <= 250) {
    aqi = (pm25 - 150.5) * (300.0 - 201.0) / (250.4 - 150.5) + 201.0;
    result.level = F("Sehr schlecht");
    result.colorCode = 0x800080;
  } else {
    aqi = (pm25 - 250.5) * (500.0 - 301.0) / (500.4 - 250.5) + 301.0;
    result.level = F("Gefährlich");
    result.colorCode = 0x7E0023;
  }

  result.aqi = aqi;
  result.success = false;
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DEBUG_PRINTLN("=== Air Quality Monitor Starting ===");
 
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
  displayManager.showMessage("System startet...");
  
  // Initialize sensors
  displayManager.showMessage("Sensoren init...");
  bool sensorsOK = sensorManager.init();
  
  if (sensorsOK) {
    displayManager.showMessage("Sensoren OK!", 1000);
  } else {
    displayManager.showMessage("Sensor Warnung!", 5000);
  }
  
  // Connect to WiFi
  displayManager.showMessage("WiFi verbinden...");
  wifiConnected = byteManager.connectWiFi();
  
  if (wifiConnected) {
    displayManager.showMessage("WiFi verbunden!", 1000);
    String ip = WiFi.localIP().toString();
    displayManager.showMessage("IP: " + ip, 2000);
    DEBUG_PRINTLN("WiFi connected successfully");
  } else {
    displayManager.showMessage("Offline Modus", 5000);
    DEBUG_PRINTLN("WiFi connection failed - offline mode");
  }

  displayManager.showMessage("System bereit!", 1000);
  DEBUG_PRINTLN("Setup completed");
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
          DEBUG_PRINTF("Received AQI from Node-RED: %.1f (%s)\n", calculatedAQI, aqiLevel.c_str());
        } else {
          nodeRedResponding = false;
          calculatedAQI = local.aqi;
          aqiLevel = local.level;
          aqiColorCode = local.colorCode;
          DEBUG_PRINTLN("Node-RED timeout or error");
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
      DEBUG_PRINTF("BSEC - CO2: %.0f ppm (acc:%d), VOC: %.1f mg/m³ (acc:%d), IAQ: %.0f (acc:%d)\n",
                   data.co2Equivalent, data.co2Accuracy,
                   data.breathVocEquivalent, data.breathVocAccuracy,
                   data.iaq, data.iaqAccuracy);
    }
  }
  
  // Check WiFi connection
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("WiFi lost - attempting reconnection");
    wifiConnected = byteManager.connectWiFi();
  }
  
  delay(100);
}