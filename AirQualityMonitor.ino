// ===== AIR QUALITY MONITOR V1.2 - INTERNAL CALCULATIONS =====
// Advanced version with BSEC LP Mode, Stealth Control, CO2/VOC, Internal Calculations
// Author: Dennis Wittke
// Version: 1.2.0 - Node-RED removed, all calculations internal, memory optimized
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

// Project includes
#include "config.h"
#include "secrets.h"
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
SensorManager sensorManager(iaqSensor, pms);
DisplayManager displayManager(u8g2, strip);
ButtonHandler buttonHandler(displayManager);
LEDManager ledManager(strip, displayManager);
WiFiManager wifiManager;
MQTTManager mqttManager;

// ===== GLOBAL VARIABLES =====
bool wifiConnected = false;
float calculatedAQI = 50.0;
const char* aqiLevel = "Good";
uint32_t aqiColorCode = 0x00FF00; // Green

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
  }

  // Check WiFi connection
  if (wifiConnected && !wifiManager.isConnected()) {
    DEBUG_WARN("WiFi lost - attempting reconnection");
    wifiConnected = wifiManager.connect();
  }
  
  // Always update LED transitions (even when no sensor update)
  ledManager.updateLEDsWithTransition(aqiColorCode);
}