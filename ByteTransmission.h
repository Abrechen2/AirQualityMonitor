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
  unsigned long lastWiFiAttempt = 0;
  int wifiRetryDelay = 5000; // Start with 5 seconds

public:
  ByteTransmissionManager();

  bool connectWiFi();
  bool reconnectWiFi();
  bool isTimeToSend();
  bool canAttemptWiFiReconnect();
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
    wifiRetryDelay = 5000; // Reset retry delay on success
    lastWiFiAttempt = millis();
    return true;
  } else {
    DEBUG_ERROR("WiFi connection failed");
    lastWiFiAttempt = millis();
    return false;
  }
}

bool ByteTransmissionManager::reconnectWiFi() {
  // Don't attempt if recently tried
  if (!canAttemptWiFiReconnect()) {
    return false;
  }

  DEBUG_INFO("Attempting WiFi reconnection (retry delay: %d ms)", wifiRetryDelay);

  // Disconnect first to clean up
  WiFi.disconnect();
  delay(1000);

  bool connected = connectWiFi();

  if (!connected) {
    // Exponential backoff up to 5 minutes
    wifiRetryDelay = min(wifiRetryDelay * 2, 300000);
    DEBUG_WARN("WiFi reconnection failed. Next attempt in %d ms", wifiRetryDelay);
  }

  return connected;
}

bool ByteTransmissionManager::canAttemptWiFiReconnect() {
  return (millis() - lastWiFiAttempt >= wifiRetryDelay);
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
  bool success = false;
  int retryDelay = HTTP_RETRY_DELAY_MS;

  for (int attempt = 0; attempt < HTTP_MAX_RETRIES; attempt++) {
    // Check WiFi connection before attempting
    if (WiFi.status() != WL_CONNECTED) {
      DEBUG_WARN("WiFi not connected, attempt %d/%d", attempt + 1, HTTP_MAX_RETRIES);
      delay(retryDelay);
      retryDelay *= 2; // Exponential backoff
      continue;
    }

    HTTPClient http;
    http.begin(NODERED_SEND_URL);
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("X-Packet-Size", String(sizeof(SensorDataPacket)));
    http.setTimeout(5000);

    DEBUG_INFO("Sending binary packet (%d bytes), attempt %d/%d",
               sizeof(SensorDataPacket), attempt + 1, HTTP_MAX_RETRIES);

    // Send binary data
    int httpResponseCode = http.POST((uint8_t*)&packet, sizeof(SensorDataPacket));

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      DEBUG_INFO("Binary data sent successfully, HTTP: %d", httpResponseCode);
      success = true;
      http.end();
      break;
    } else if (httpResponseCode > 0) {
      DEBUG_ERROR("HTTP POST failed: %d (attempt %d/%d)",
                  httpResponseCode, attempt + 1, HTTP_MAX_RETRIES);
    } else {
      DEBUG_ERROR("HTTP POST connection error: %d (attempt %d/%d)",
                  httpResponseCode, attempt + 1, HTTP_MAX_RETRIES);
    }

    http.end();

    // Wait before retry (except on last attempt)
    if (attempt < HTTP_MAX_RETRIES - 1) {
      delay(retryDelay);
      retryDelay *= 2; // Exponential backoff
    }
  }

  if (!success) {
    DEBUG_ERROR("Binary data transmission failed after %d attempts", HTTP_MAX_RETRIES);
  }

  return success;
}

AQIResult ByteTransmissionManager::getCalculatedAQI(const SensorData& data) {
  AQIResult result;

  // Check WiFi connection before attempting
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_WARN("WiFi not connected, skipping AQI request");
    return result;
  }

  HTTPClient http;
  http.begin(NODERED_AQI_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  http.setReuse(false);
  const char* headerKeys[] = {"Content-Length"};
  http.collectHeaders(headerKeys, 1);

  // Build JSON request with input validation
  StaticJsonDocument<256> requestDoc;

  // Validate and clamp sensor values before sending
  requestDoc["pm2_5"] = min((int)data.pm2_5, (int)PM_MAX_VALID);
  requestDoc["pm10"] = min((int)data.pm10, (int)PM_MAX_VALID);
  requestDoc["iaq"] = min(data.iaq, (float)IAQ_MAX_VALID);
  requestDoc["co2"] = constrain(data.co2Equivalent, (float)CO2_MIN_VALID, (float)CO2_MAX_VALID);
  requestDoc["calibrated"] = data.bsecCalibrated;

  String request;
  size_t serializedSize = serializeJson(requestDoc, request);

  if (serializedSize == 0) {
    DEBUG_ERROR("JSON serialization failed");
    http.end();
    return result;
  }

  DEBUG_INFO("Sending AQI request (%d bytes)", serializedSize);
  int httpResponseCode = http.POST(request);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    int responseSize = http.getSize();

    // Validate response size
    if (responseSize > HTTP_RESPONSE_MAX_SIZE) {
      DEBUG_ERROR("Response too large: %d bytes (max %d)", responseSize, HTTP_RESPONSE_MAX_SIZE);
      http.end();
      return result;
    }

    if (responseSize <= 0) {
      DEBUG_WARN("Empty or unknown response size");
    }

    String response = http.getString();

    // Additional size check after reading
    if (response.length() > HTTP_RESPONSE_MAX_SIZE) {
      DEBUG_ERROR("Response string too large: %d bytes", response.length());
      http.end();
      return result;
    }

    DEBUG_INFO("AQI response (%d bytes): %s", response.length(), response.c_str());

    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error) {
      JsonObject aqi = responseDoc["aqi"];
      if (!aqi.isNull()) {
        // Validate AQI value
        float aqiValue = aqi["combined"].as<float>();
        if (aqiValue >= 0 && aqiValue <= 1000) {
          result.aqi = aqiValue;
        } else {
          DEBUG_WARN("AQI value out of range: %.1f", aqiValue);
          result.aqi = 50.0; // Safe default
        }

        // Extract level with validation
        if (aqi.containsKey("level")) {
          const char* levelPtr = aqi["level"];
          if (levelPtr != nullptr) {
            result.level = String(levelPtr);
            // Limit level string length
            if (result.level.length() > 32) {
              result.level = result.level.substring(0, 32);
            }
          }
        }

        // Extract and parse color
        if (aqi.containsKey("color")) {
          const char* colorPtr = aqi["color"];
          if (colorPtr != nullptr) {
            result.colorCode = parseColorCode(String(colorPtr));
          }
        }

        result.success = true;
        DEBUG_INFO("Parsed AQI: %.1f (%s)", result.aqi, result.level.c_str());
      } else {
        DEBUG_ERROR("Missing 'aqi' object in response");
      }
    } else {
      DEBUG_ERROR("JSON parse failed: %s", error.c_str());
    }
  } else if (httpResponseCode > 0) {
    DEBUG_ERROR("AQI request failed, HTTP: %d", httpResponseCode);
  } else {
    DEBUG_ERROR("AQI request connection error: %d", httpResponseCode);
  }

  http.end();
  return result;
}

uint32_t ByteTransmissionManager::parseColorCode(const String& colorStr) {
  // Validate input string is not empty
  if (colorStr.length() == 0) {
    DEBUG_WARN("Empty color string, using default green");
    return 0x00FF00;
  }

  // Parse hex color code with validation
  if (colorStr.startsWith("#") && colorStr.length() == 7) {
    // Validate all characters after # are hex digits
    String hexPart = colorStr.substring(1);
    for (unsigned int i = 0; i < hexPart.length(); i++) {
      char c = hexPart.charAt(i);
      if (!isxdigit(c)) {
        DEBUG_WARN("Invalid hex color: %s", colorStr.c_str());
        return 0x00FF00;
      }
    }

    char* endPtr;
    long color = strtol(hexPart.c_str(), &endPtr, 16);

    // Verify entire string was parsed
    if (*endPtr != '\0') {
      DEBUG_WARN("Invalid color format: %s", colorStr.c_str());
      return 0x00FF00;
    }

    // Validate color is within valid RGB range
    if (color < 0 || color > 0xFFFFFF) {
      DEBUG_WARN("Color out of range: %s", colorStr.c_str());
      return 0x00FF00;
    }

    return (uint32_t)color;
  }

  // Fallback colors based on level names
  if (colorStr.indexOf("Good") >= 0) return 0x00FF00;
  if (colorStr.indexOf("Moderate") >= 0) return 0xFFFF00;
  if (colorStr.indexOf("Unhealthy") >= 0) return 0xFF0000;

  DEBUG_WARN("Unknown color format: %s, using default", colorStr.c_str());
  return 0x00FF00; // Default green
}

#endif
