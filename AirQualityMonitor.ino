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
bool nodeRedResponding = true;  // Node-RED Response Status
float calculatedAQI = 50.0;
String aqiLevel = "Gut";
uint32_t aqiColorCode = 0x00FF00; // Green

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DEBUG_PRINTLN("=== Air Quality Monitor V6 Starting ===");
  DEBUG_PRINTLN("BSEC ULP + Byte Transmission + DS18B20 Main Temp");
  
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
  displayManager.showMessage("Sensoren initialisieren...");
  bool sensorsOK = sensorManager.init();
  
  if (sensorsOK) {
    displayManager.showMessage("Sensoren OK!", 1000);
  } else {
    displayManager.showMessage("Sensor Warnung!", 2000);
  }
  
  // Connect to WiFi
  displayManager.showMessage("WiFi verbinden...");
  wifiConnected = byteManager.connectWiFi();
  
  if (wifiConnected) {
    displayManager.showMessage("WiFi verbunden!", 2000);
    DEBUG_PRINTLN("WiFi connected successfully");
  } else {
    displayManager.showMessage("WiFi Fehler!", 2000);
    DEBUG_PRINTLN("WiFi connection failed - offline mode");
  }
  
  // Byte transmission info
  displayManager.showMessage("Byte Übertragung...");
  displayManager.showMessage("Struktur: 44 Bytes!");
  displayManager.showMessage("4+24+3+7+5+1");
  delay(2000);
  
  displayManager.showMessage("System bereit!", 1000);
  DEBUG_PRINTLN("Setup completed - starting main loop");
}

void loop() {
  // Update system components
  buttonHandler.update();
  
  // Read sensors
  if (sensorManager.update()) {
    SensorData data = sensorManager.getData();
    
    // Send data to Node-RED and get calculated AQI back
    if (wifiConnected && byteManager.isTimeToSend()) {
      AQIResult result = byteManager.sendDataAndGetAQI(data);
      if (result.success) {
        calculatedAQI = result.aqi;
        aqiLevel = result.level;
        aqiColorCode = result.colorCode;
        nodeRedResponding = true;  // Node-RED antwortet
        DEBUG_PRINTF("Received AQI from Node-RED: %.1f (%s)\n", calculatedAQI, aqiLevel.c_str());
      } else {
        nodeRedResponding = false;  // Node-RED antwortet nicht
        DEBUG_PRINTLN("Node-RED timeout or error");
      }
    }
    
    // Update display with current view
    displayManager.updateDisplay(data, calculatedAQI, aqiLevel, wifiConnected, nodeRedResponding);
    
    // Update LEDs with AQI color (respects stealth mode)
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