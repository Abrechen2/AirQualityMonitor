#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "SensorManager.h"

// ===== AQI RESULT STRUCTURE =====
struct AQIResult {
  bool success = false;
  float aqi = 50.0;
  String level = "Gut";
  uint32_t colorCode = 0x00FF00; // Green
};

// ===== WIFI MANAGER CLASS =====
class WiFiManager {
private:
  unsigned long lastSendTime = 0;
  
public:
  WiFiManager();
  
  bool connectWiFi();
  bool isTimeToSend();
  AQIResult sendDataAndGetAQI(const SensorData& data);
  
private:
  bool sendSensorData(const SensorData& data);
  AQIResult getCalculatedAQI(const SensorData& data);
  uint32_t parseColorCode(const String& colorStr);
};

// ===== IMPLEMENTATION =====
WiFiManager::WiFiManager() {
}

bool WiFiManager::connectWiFi() {
  DEBUG_PRINTLN("Connecting to WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && 
         (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    DEBUG_PRINT(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN();
    DEBUG_PRINTF("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    DEBUG_PRINTF("RSSI: %d dBm\n", WiFi.RSSI());
    return true;
  } else {
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("WiFi connection failed");
    return false;
  }
}

bool WiFiManager::isTimeToSend() {
  return (millis() - lastSendTime >= DATA_SEND_INTERVAL);
}

AQIResult WiFiManager::sendDataAndGetAQI(const SensorData& data) {
  AQIResult result;
  
  // Sensordaten an Node-RED senden
  if (sendSensorData(data)) {
    // Berechnete AQI von Node-RED abrufen
    result = getCalculatedAQI(data);
    lastSendTime = millis();
  }
  
  return result;
}

bool WiFiManager::sendSensorData(const SensorData& data) {
  HTTPClient http;
  http.begin(NODERED_SEND_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  // JSON erstellen - erweitert mit BSEC-Daten
  DynamicJsonDocument doc(1536); // Größer für mehr Daten
  
  // Timestamp
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  
  // BME68X BSEC Daten
  if (data.bme68xAvailable) {
    doc["bme68x"]["temperature"] = data.temperature;
    doc["bme68x"]["humidity"] = data.humidity;
    doc["bme68x"]["pressure"] = data.pressure;
    doc["bme68x"]["gas_resistance"] = data.gasResistance;
    doc["bme68x"]["iaq"] = data.iaq;
    doc["bme68x"]["iaq_accuracy"] = data.iaqAccuracy;
    doc["bme68x"]["static_iaq"] = data.staticIaq;
    doc["bme68x"]["static_iaq_accuracy"] = data.staticIaqAccuracy;
    doc["bme68x"]["co2_equivalent"] = data.co2Equivalent;
    doc["bme68x"]["co2_accuracy"] = data.co2Accuracy;
    doc["bme68x"]["breath_voc_equivalent"] = data.breathVocEquivalent;
    doc["bme68x"]["breath_voc_accuracy"] = data.breathVocAccuracy;
    doc["bme68x"]["calibrated"] = data.bsecCalibrated;
    doc["bme68x"]["available"] = true;
  } else {
    doc["bme68x"]["available"] = false;
  }
  
  // DS18B20 Daten
  if (data.ds18b20Available) {
    doc["ds18b20"]["temperature"] = data.externalTemp;
    doc["ds18b20"]["available"] = true;
  } else {
    doc["ds18b20"]["available"] = false;
  }
  
  // PMS5003 Daten
  if (data.pms5003Available) {
    doc["pms5003"]["pm1_0"] = data.pm1_0;
    doc["pms5003"]["pm2_5"] = data.pm2_5;
    doc["pms5003"]["pm10"] = data.pm10;
    doc["pms5003"]["available"] = true;
  } else {
    doc["pms5003"]["available"] = false;
  }
  
  // System Daten
  doc["system"]["free_heap"] = ESP.getFreeHeap();
  doc["system"]["wifi_rssi"] = WiFi.RSSI();
  doc["system"]["wifi_connected"] = true;
  
  // JSON zu String
  String jsonString;
  serializeJson(doc, jsonString);
  
  DEBUG_PRINTF("Sending enhanced JSON (%d bytes)\n", jsonString.length());
  
  int httpResponseCode = http.POST(jsonString);
  
  bool success = false;
  if (httpResponseCode > 0) {
    DEBUG_PRINTF("Data sent successfully, HTTP: %d\n", httpResponseCode);
    success = true;
  } else {
    DEBUG_PRINTF("HTTP POST failed: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}

AQIResult WiFiManager::getCalculatedAQI(const SensorData& data) {
  AQIResult result;
  
  HTTPClient http;
  http.begin(NODERED_AQI_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  // Anfrage für AQI-Berechnung - erweitert
  DynamicJsonDocument requestDoc(768);
  requestDoc["pm1_0"] = data.pm1_0;
  requestDoc["pm2_5"] = data.pm2_5;
  requestDoc["pm10"] = data.pm10;
  requestDoc["iaq"] = data.iaq;
  requestDoc["static_iaq"] = data.staticIaq;
  requestDoc["co2_equivalent"] = data.co2Equivalent;
  requestDoc["breath_voc_equivalent"] = data.breathVocEquivalent;
  requestDoc["gas_resistance"] = data.gasResistance;
  requestDoc["calibrated"] = data.bsecCalibrated;
  
  String requestString;
  serializeJson(requestDoc, requestString);
  
  int httpResponseCode = http.POST(requestString);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    DEBUG_PRINTF("AQI Response: %s\n", response.c_str());
    
    DynamicJsonDocument responseDoc(512);
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      result.success = true;
      result.aqi = responseDoc["aqi"] | 50.0;
      result.level = responseDoc["level"] | "Gut";
      
      String colorStr = responseDoc["color"] | "#00FF00";
      result.colorCode = parseColorCode(colorStr);
      
      DEBUG_PRINTF("Parsed AQI: %.1f, Level: %s, Color: 0x%06X\n", 
                   result.aqi, result.level.c_str(), result.colorCode);
    } else {
      DEBUG_PRINTLN("JSON parsing failed for AQI response");
    }
  } else {
    DEBUG_PRINTF("AQI request failed: %d\n", httpResponseCode);
  }
  
  http.end();
  return result;
}

uint32_t WiFiManager::parseColorCode(const String& colorStr) {
  // Parse #RRGGBB format
  if (colorStr.startsWith("#") && colorStr.length() == 7) {
    long color = strtol(colorStr.substring(1).c_str(), NULL, 16);
    return (uint32_t)color;
  }
  
  // Fallback zu Standard-Farben basierend auf Level
  if (colorStr.indexOf("green") >= 0 || colorStr.indexOf("Gut") >= 0) return 0x00FF00;
  if (colorStr.indexOf("yellow") >= 0 || colorStr.indexOf("Mäßig") >= 0) return 0xFFFF00;
  if (colorStr.indexOf("orange") >= 0 || colorStr.indexOf("Empfindliche") >= 0) return 0xFF8000;
  if (colorStr.indexOf("red") >= 0 || colorStr.indexOf("Ungesund") >= 0) return 0xFF0000;
  if (colorStr.indexOf("purple") >= 0 || colorStr.indexOf("ungesund") >= 0) return 0x800080;
  if (colorStr.indexOf("maroon") >= 0 || colorStr.indexOf("Gefährlich") >= 0) return 0x800000;
  
  return 0x00FF00; // Default green
}

#endif