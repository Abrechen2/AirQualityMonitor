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
  bool isTimeToSend() const;
  AQIResult sendDataAndGetAQI(const SensorData& data);
  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  
private:
  SensorDataPacket createPacket(const SensorData& data);
  uint8_t calculateChecksum(const SensorDataPacket& packet);
  bool sendBinaryData(const SensorDataPacket& packet);
  AQIResult getCalculatedAQI(const SensorData& data);
  uint32_t parseColorCode(const String& colorStr);
  
  // Safe conversion functions with overflow protection
  int16_t safeTempToInt16(float temp);
  uint16_t safeHumidityToUint16(float humidity);
  uint16_t safePressureToUint16(float pressure);
  uint16_t safeIAQToUint16(float iaq);
  uint16_t safeVOCToUint16(float voc);
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

bool ByteTransmissionManager::isTimeToSend() const {
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

  // BME68X data with overflow protection
  if (data.bme68xAvailable) {
    packet.bme_temperature = safeTempToInt16(data.temperature);
    packet.bme_humidity = safeHumidityToUint16(data.humidity);
    packet.bme_pressure = safePressureToUint16(data.pressure);
    packet.gas_resistance = (uint32_t)data.gasResistance;
    packet.iaq = safeIAQToUint16(data.iaq);
    packet.static_iaq = safeIAQToUint16(data.staticIaq);
    // CO2 can be up to 40000, but uint16_t max is 65535, so safe
    packet.co2_equivalent = (data.co2Equivalent > 65535) ? 65535 : (uint16_t)data.co2Equivalent;
    packet.breath_voc = safeVOCToUint16(data.breathVocEquivalent);
    packet.iaq_accuracy = data.iaqAccuracy;
    packet.co2_accuracy = data.co2Accuracy;
    packet.voc_accuracy = data.breathVocAccuracy;
    packet.bme_flags = (data.bme68xAvailable ? 1 : 0) | (data.bsecCalibrated ? 2 : 0);
  }
  
  // DS18B20 data with overflow protection
  if (data.ds18b20Available) {
    packet.ds_temperature = safeTempToInt16(data.externalTemp);
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
  if (!isConnected()) {
    DEBUG_ERROR("WiFi not connected - cannot send data");
    return false;
  }

  HTTPClient http;
  if (!http.begin(NODERED_SEND_URL)) {
    DEBUG_ERROR("HTTP begin failed - invalid URL?");
    return false;
  }

  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Packet-Size", String(sizeof(SensorDataPacket)));
  http.setTimeout(HTTP_TIMEOUT_MS);

  DEBUG_INFO("Sending binary packet (%d bytes)", sizeof(SensorDataPacket));

  // Send binary data
  int httpResponseCode = http.POST((uint8_t*)&packet, sizeof(SensorDataPacket));

  bool success = false;
  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    DEBUG_INFO("Binary data sent successfully, HTTP: %d", httpResponseCode);
    success = true;
  } else if (httpResponseCode > 0) {
    DEBUG_ERROR("HTTP POST failed with code: %d", httpResponseCode);
  } else {
    DEBUG_ERROR("HTTP POST failed - connection error: %d", httpResponseCode);
  }

  http.end();
  return success;
}

AQIResult ByteTransmissionManager::getCalculatedAQI(const SensorData& data) {
  AQIResult result;

  if (!isConnected()) {
    DEBUG_ERROR("WiFi not connected - cannot get AQI");
    return result;
  }

  HTTPClient http;
  if (!http.begin(NODERED_AQI_URL)) {
    DEBUG_ERROR("HTTP begin failed for AQI - invalid URL?");
    return result;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(HTTP_AQI_TIMEOUT_MS);
  http.setReuse(false);
  const char* headerKeys[] = {"Content-Length"};
  http.collectHeaders(headerKeys, 1);

  // Validate input data before sending
  if (data.pm2_5 > PM_MAX || data.pm10 > PM_MAX || 
      data.iaq > IAQ_MAX || data.co2Equivalent > CO2_MAX) {
    DEBUG_WARN("Invalid sensor data for AQI request - skipping");
    http.end();
    return result;
  }

  // Build JSON request safely
  StaticJsonDocument<JSON_MAX_SIZE> doc;
  doc["pm2_5"] = data.pm2_5;
  doc["pm10"] = data.pm10;
  doc["iaq"] = data.iaq;
  doc["co2"] = data.co2Equivalent;
  doc["calibrated"] = data.bsecCalibrated;

  String request;
  if (serializeJson(doc, request) == 0) {
    DEBUG_ERROR("JSON serialization failed");
    http.end();
    return result;
  }

  // Validate request size
  if (request.length() > JSON_MAX_SIZE) {
    DEBUG_ERROR("JSON request too large: %d bytes", request.length());
    http.end();
    return result;
  }

  int httpResponseCode = http.POST(request);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    // Check response size before reading
    int contentLength = http.getSize();
    if (contentLength <= 0 || contentLength > HTTP_MAX_RESPONSE_SIZE) {
      DEBUG_ERROR("Invalid response size: %d bytes", contentLength);
      http.end();
      return result;
    }

    String response = http.getString();
    
    // Validate response string length
    if (response.length() > HTTP_MAX_RESPONSE_SIZE) {
      DEBUG_ERROR("Response string too large: %d bytes", response.length());
      http.end();
      return result;
    }

    DEBUG_INFO("Full AQI response: %s", response.c_str());

    StaticJsonDocument<JSON_MAX_SIZE> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      DEBUG_ERROR("JSON parse failed: %s", error.c_str());
      http.end();
      return result;
    }

    // Validate JSON structure
    if (!responseDoc.is<JsonObject>()) {
      DEBUG_ERROR("Invalid JSON structure - expected object");
      http.end();
      return result;
    }

    JsonObject aqi = responseDoc["aqi"];
    if (aqi.isNull()) {
      DEBUG_ERROR("Missing 'aqi' field in response");
      http.end();
      return result;
    }

    // Validate and extract AQI value
    if (aqi.containsKey("combined")) {
      float aqiValue = aqi["combined"].as<float>();
      if (aqiValue >= 0 && aqiValue <= 500) {
        result.aqi = aqiValue;
      } else {
        DEBUG_WARN("Invalid AQI value: %.2f", aqiValue);
        http.end();
        return result;
      }
    } else {
      DEBUG_ERROR("Missing 'combined' field in AQI response");
      http.end();
      return result;
    }

    // Extract level (optional)
    if (aqi.containsKey("level") && aqi["level"].is<const char*>()) {
      const char* levelStr = aqi["level"];
      if (levelStr != nullptr && strlen(levelStr) < 64) {
        result.level = String(levelStr);
      }
    }

    // Extract color (optional)
    if (aqi.containsKey("color") && aqi["color"].is<const char*>()) {
      const char* colorStr = aqi["color"];
      if (colorStr != nullptr) {
        result.colorCode = parseColorCode(String(colorStr));
      }
    }

    result.success = true;
    DEBUG_INFO("Final parsed AQI: %.1f (%s)", result.aqi, result.level.c_str());
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

// Safe conversion functions with overflow protection
int16_t ByteTransmissionManager::safeTempToInt16(float temp) {
  int32_t tempScaled = (int32_t)(temp * 100.0f);
  if (tempScaled > INT16_TEMP_MAX) return INT16_TEMP_MAX;
  if (tempScaled < INT16_TEMP_MIN) return INT16_TEMP_MIN;
  return (int16_t)tempScaled;
}

uint16_t ByteTransmissionManager::safeHumidityToUint16(float humidity) {
  uint32_t humidityScaled = (uint32_t)(humidity * 100.0f);
  if (humidityScaled > UINT16_HUMIDITY_MAX) return UINT16_HUMIDITY_MAX;
  return (uint16_t)humidityScaled;
}

uint16_t ByteTransmissionManager::safePressureToUint16(float pressure) {
  uint32_t pressureScaled = (uint32_t)(pressure * 10.0f);
  if (pressureScaled > UINT16_PRESSURE_MAX) return UINT16_PRESSURE_MAX;
  return (uint16_t)pressureScaled;
}

uint16_t ByteTransmissionManager::safeIAQToUint16(float iaq) {
  uint32_t iaqScaled = (uint32_t)(iaq * 10.0f);
  if (iaqScaled > UINT16_IAQ_MAX) return UINT16_IAQ_MAX;
  return (uint16_t)iaqScaled;
}

uint16_t ByteTransmissionManager::safeVOCToUint16(float voc) {
  uint32_t vocScaled = (uint32_t)(voc * 100.0f);
  if (vocScaled > UINT16_VOC_MAX) return UINT16_VOC_MAX;
  return (uint16_t)vocScaled;
}

#endif
