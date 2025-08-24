#ifndef BYTE_TRANSMISSION_H
#define BYTE_TRANSMISSION_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "SensorManager.h"
#include "TimeUtils.h"

// ===== BYTE TRANSMISSION PROTOCOL =====
// Compact binary format for minimal data transfer

#pragma pack(push, 1)  // No padding bytes

struct SensorDataPacket {
  // Header (4 bytes)
  uint32_t timestamp;           // Unix timestamp
  
  // BME68X Data (22 bytes)
  int16_t bme_temperature;      // °C * 100 (e.g., 2350 = 23.50°C)
  uint16_t bme_humidity;        // % * 100 (e.g., 4520 = 45.20%)
  uint16_t bme_pressure;        // hPa * 10 (e.g., 10132 = 1013.2 hPa)
  uint32_t gas_resistance;      // Ohm
  uint16_t iaq;                 // IAQ * 10
  uint16_t static_iaq;          // Static IAQ * 10
  uint16_t co2_equivalent;      // ppm
  uint16_t breath_voc;          // mg/m³ * 100
  uint8_t iaq_accuracy;         // 0-3
  uint8_t co2_accuracy;         // 0-3
  uint8_t voc_accuracy;         // 0-3
  uint8_t bme_flags;            // Bit 0: available, Bit 1: calibrated
  
  // DS18B20 Data (3 bytes)
  int16_t ds_temperature;       // °C * 100
  uint8_t ds_flags;             // Bit 0: available
  
  // PMS5003 Data (7 bytes)
  uint16_t pm1_0;               // µg/m³
  uint16_t pm2_5;               // µg/m³
  uint16_t pm10;                // µg/m³
  uint8_t pms_flags;            // Bit 0: available
  
  // System data (5 bytes)
  uint32_t uptime_seconds;      // Seconds since start (4 bytes)
  int8_t wifi_rssi;             // dBm (1 byte)
  
  // Checksum (1 byte)
  uint8_t checksum;             // XOR of all bytes
};
// TOTAL: 4+22+3+7+5+1 = 42 bytes

#pragma pack(pop)

// ===== AQI RESULT STRUCTURE =====
struct AQIResult {
  bool success = false;
  float aqi = 50.0;
  String level = "Good";
  uint32_t colorCode = 0x00FF00;
};

// ===== BYTE TRANSMISSION MANAGER =====
class ByteTransmissionManager {
private:
  unsigned long lastSendTime = 0;
  
public:
  ByteTransmissionManager();
  
  bool connectWiFi();
  bool isTimeToSend();
  AQIResult sendDataAndGetAQI(const SensorData& data);
  
private:
  SensorDataPacket createPacket(const SensorData& data);
  uint8_t calculateChecksum(const SensorDataPacket& packet);
  bool sendBinaryData(const SensorDataPacket& packet);
  AQIResult getCalculatedAQI(const SensorData& data);
  uint32_t parseColorCode(const String& colorStr);
};

// ===== IMPLEMENTATION =====
ByteTransmissionManager::ByteTransmissionManager() {
}

bool ByteTransmissionManager::connectWiFi() {
  DEBUG_INFO("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_INFO("WiFi connected: %s", WiFi.localIP().toString().c_str());
    DEBUG_INFO("RSSI: %d dBm", WiFi.RSSI());
    return true;
  } else {
    DEBUG_ERROR("WiFi connection failed");
    return false;
  }
}

bool ByteTransmissionManager::isTimeToSend() {
  return (millis() - lastSendTime >= DATA_SEND_INTERVAL);
}

AQIResult ByteTransmissionManager::sendDataAndGetAQI(const SensorData& data) {
  AQIResult result;
  
  // Send binary sensor data to Node-RED
  SensorDataPacket packet = createPacket(data);
  if (sendBinaryData(packet)) {
    // Retrieve AQI from Node-RED (JSON)
    result = getCalculatedAQI(data);
    lastSendTime = millis();
  }
  
  return result;
}

SensorDataPacket ByteTransmissionManager::createPacket(const SensorData& data) {
  SensorDataPacket packet = {0};
  
  // Header
  packet.timestamp = (uint32_t)(getUptimeMillis() / 1000);  // Unix-like since start

  // BME68X data
  if (data.bme68xAvailable) {
    packet.bme_temperature = (int16_t)(data.temperature * 100);
    packet.bme_humidity = (uint16_t)(data.humidity * 100);
    packet.bme_pressure = (uint16_t)(data.pressure * 10);
    packet.gas_resistance = (uint32_t)data.gasResistance;
    packet.iaq = (uint16_t)(data.iaq * 10);
    packet.static_iaq = (uint16_t)(data.staticIaq * 10);
    packet.co2_equivalent = (uint16_t)data.co2Equivalent;
    packet.breath_voc = (uint16_t)(data.breathVocEquivalent * 100);
    packet.iaq_accuracy = data.iaqAccuracy;
    packet.co2_accuracy = data.co2Accuracy;
    packet.voc_accuracy = data.breathVocAccuracy;
    packet.bme_flags = (data.bme68xAvailable ? 1 : 0) | (data.bsecCalibrated ? 2 : 0);
  }
  
  // DS18B20 data
  if (data.ds18b20Available) {
    packet.ds_temperature = (int16_t)(data.externalTemp * 100);
    packet.ds_flags = 1;  // Available
  }
  
  // PMS5003 data
  if (data.pms5003Available) {
    packet.pm1_0 = data.pm1_0;
    packet.pm2_5 = data.pm2_5;
    packet.pm10 = data.pm10;
    packet.pms_flags = 1;  // Available
  }
  
  // System data
  packet.uptime_seconds = (uint32_t)(getUptimeMillis() / 1000);  // Seconds since start
  packet.wifi_rssi = (int8_t)WiFi.RSSI();

  // Calculate checksum
  packet.checksum = calculateChecksum(packet);
  
  return packet;
}

uint8_t ByteTransmissionManager::calculateChecksum(const SensorDataPacket& packet) {
  uint8_t checksum = 0;
  const uint8_t* bytes = (const uint8_t*)&packet;
  
  // XOR all bytes except checksum
  for (size_t i = 0; i < sizeof(SensorDataPacket) - 1; i++) {
    checksum ^= bytes[i];
  }
  
  return checksum;
}

bool ByteTransmissionManager::sendBinaryData(const SensorDataPacket& packet) {
  HTTPClient http;
  http.begin(NODERED_SEND_URL);
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Packet-Size", String(sizeof(SensorDataPacket)));
  http.setTimeout(5000);
  
  DEBUG_INFO("Sending binary packet (%d bytes)", sizeof(SensorDataPacket));
  
  // Send binary data
  int httpResponseCode = http.POST((uint8_t*)&packet, sizeof(SensorDataPacket));
  
  bool success = false;
  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    DEBUG_INFO("Binary data sent successfully, HTTP: %d", httpResponseCode);
    success = true;
  } else {
    DEBUG_ERROR("HTTP POST failed: %d", httpResponseCode);
  }
  
  http.end();
  return success;
}

AQIResult ByteTransmissionManager::getCalculatedAQI(const SensorData& data) {
  AQIResult result;
  
  HTTPClient http;
  http.begin(NODERED_AQI_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  // Small JSON request for AQI calculation
  String request = "{\"pm2_5\":" + String(data.pm2_5) +
                   ",\"pm10\":" + String(data.pm10) +
                   ",\"iaq\":" + String(data.iaq) +
                   ",\"co2\":" + String(data.co2Equivalent) +
                   ",\"calibrated\":" + (data.bsecCalibrated ? "true" : "false") + "}";

  int httpResponseCode = http.POST(request);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    String response = http.getString();
    DEBUG_INFO("Full AQI response: %s", response.c_str());

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      JsonObject aqi = doc["aqi"];
      if (!aqi.isNull()) {
        result.aqi = aqi["combined"].as<float>();
        if (aqi.containsKey("level")) {
          result.level = String((const char*)aqi["level"]);
        }
        if (aqi.containsKey("color")) {
          result.colorCode = parseColorCode(String((const char*)aqi["color"]));
        }
        result.success = true;
        DEBUG_INFO("Final parsed AQI: %.1f (%s)", result.aqi, result.level.c_str());
      }
    } else {
      DEBUG_ERROR("JSON parse failed: %s", error.c_str());
    }
  } else {
    DEBUG_ERROR("AQI request failed, HTTP: %d", httpResponseCode);
  }

  http.end();
  return result;
}

uint32_t ByteTransmissionManager::parseColorCode(const String& colorStr) {
  if (colorStr.startsWith("#") && colorStr.length() == 7) {
    long color = strtol(colorStr.substring(1).c_str(), NULL, 16);
    return (uint32_t)color;
  }
  
  // Fallback colors
  if (colorStr.indexOf("Good") >= 0) return 0x00FF00;
  if (colorStr.indexOf("Moderate") >= 0) return 0xFFFF00;
  if (colorStr.indexOf("Unhealthy") >= 0) return 0xFF0000;

  return 0x00FF00; // Default green
}

#endif