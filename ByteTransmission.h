#ifndef BYTE_TRANSMISSION_H
#define BYTE_TRANSMISSION_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "secrets.h"
#include "SensorManager.h"

// ===== BYTE TRANSMISSION PROTOCOL =====
// Kompaktes Binärformat für minimale Datenübertragung

#pragma pack(push, 1)  // Keine Padding-Bytes

struct SensorDataPacket {
  // Header (4 bytes)
  uint32_t timestamp;           // Unix timestamp
  
  // BME68X Data (24 bytes)
  int16_t bme_temperature;      // °C * 100 (z.B. 2350 = 23.50°C)
  uint16_t bme_humidity;        // % * 100 (z.B. 4520 = 45.20%)
  uint16_t bme_pressure;        // hPa * 10 (z.B. 10132 = 1013.2 hPa)
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
  
  // System Data (5 bytes) - zurück zu Sekunden
  uint32_t uptime_seconds;      // Sekunden seit Start (4 bytes)
  int8_t wifi_rssi;             // dBm (1 byte)
  
  // Checksumme (1 byte)
  uint8_t checksum;             // XOR aller Bytes
};
// TOTAL: 4+24+3+7+5+1 = 44 bytes

#pragma pack(pop)

// ===== AQI RESULT STRUCTURE (unverändert) =====
struct AQIResult {
  bool success = false;
  float aqi = 50.0;
  String level = "Gut";
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

bool ByteTransmissionManager::isTimeToSend() {
  return (millis() - lastSendTime >= DATA_SEND_INTERVAL);
}

AQIResult ByteTransmissionManager::sendDataAndGetAQI(const SensorData& data) {
  AQIResult result;
  
  // Binäre Sensordaten an Node-RED senden
  SensorDataPacket packet = createPacket(data);
  if (sendBinaryData(packet)) {
    // AQI von Node-RED abrufen (weiterhin JSON)
    result = getCalculatedAQI(data);
    lastSendTime = millis();
  }
  
  return result;
}

SensorDataPacket ByteTransmissionManager::createPacket(const SensorData& data) {
  SensorDataPacket packet = {0};
  
  // Header
  packet.timestamp = millis() / 1000;  // Unix-ähnlich
  
  // BME68X Daten
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
  
  // DS18B20 Daten
  if (data.ds18b20Available) {
    packet.ds_temperature = (int16_t)(data.externalTemp * 100);
    packet.ds_flags = 1;  // Available
  }
  
  // PMS5003 Daten
  if (data.pms5003Available) {
    packet.pm1_0 = data.pm1_0;
    packet.pm2_5 = data.pm2_5;
    packet.pm10 = data.pm10;
    packet.pms_flags = 1;  // Available
  }
  
  // System Daten
  packet.uptime_seconds = millis() / 1000;  // Sekunden seit Start
  packet.wifi_rssi = (int8_t)WiFi.RSSI();
  
  // Checksumme berechnen
  packet.checksum = calculateChecksum(packet);
  
  return packet;
}

uint8_t ByteTransmissionManager::calculateChecksum(const SensorDataPacket& packet) {
  uint8_t checksum = 0;
  const uint8_t* bytes = (const uint8_t*)&packet;
  
  // Alle Bytes außer Checksumme XOR-en
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
  
  DEBUG_PRINTF("Sending binary packet (%d bytes)\n", sizeof(SensorDataPacket));
  
  // Binäre Daten senden
  int httpResponseCode = http.POST((uint8_t*)&packet, sizeof(SensorDataPacket));
  
  bool success = false;
  if (httpResponseCode > 0) {
    DEBUG_PRINTF("Binary data sent successfully, HTTP: %d\n", httpResponseCode);
    success = true;
  } else {
    DEBUG_PRINTF("HTTP POST failed: %d\n", httpResponseCode);
  }
  
  http.end();
  return success;
}

// KORRIGIERTER ESP32 CODE für getCalculatedAQI:
AQIResult ByteTransmissionManager::getCalculatedAQI(const SensorData& data) {
  AQIResult result;
  
  HTTPClient http;
  http.begin(NODERED_AQI_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);
  
  // Kleine JSON-Anfrage für AQI-Berechnung
  String request = "{\"pm2_5\":" + String(data.pm2_5) + 
                   ",\"pm10\":" + String(data.pm10) + 
                   ",\"iaq\":" + String(data.iaq) + 
                   ",\"co2\":" + String(data.co2Equivalent) + 
                   ",\"calibrated\":" + (data.bsecCalibrated ? "true" : "false") + "}";
  
  int httpResponseCode = http.POST(request);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    DEBUG_PRINTF("Full AQI Response: %s\n", response.c_str());
    
    // KORRIGIERT: Suche nach dem nested "combined" Wert
    int combinedPos = response.indexOf("\"combined\":");
    int levelPos = response.indexOf("\"level\":\"");
    int colorPos = response.indexOf("\"color\":\"");
    
    if (combinedPos > 0) {
      int start = combinedPos + 11; // "combined": hat 11 Zeichen
      int end = response.indexOf(',', start);
      if (end == -1) end = response.indexOf('}', start);
      
      String aqiStr = response.substring(start, end);
      result.aqi = aqiStr.toFloat();
      result.success = true;
      
      DEBUG_PRINTF("Extracted AQI: %s -> %.1f\n", aqiStr.c_str(), result.aqi);
    }
    
    if (levelPos > 0) {
      int start = levelPos + 9;
      int end = response.indexOf('\"', start);
      result.level = response.substring(start, end);
    }
    
    if (colorPos > 0) {
      int start = colorPos + 9;
      int end = response.indexOf('\"', start);
      String colorStr = response.substring(start, end);
      result.colorCode = parseColorCode(colorStr);
    }
    
    DEBUG_PRINTF("Final parsed AQI: %.1f (%s)\n", result.aqi, result.level.c_str());
  }
  
  http.end();
  return result;
}

uint32_t ByteTransmissionManager::parseColorCode(const String& colorStr) {
  if (colorStr.startsWith("#") && colorStr.length() == 7) {
    long color = strtol(colorStr.substring(1).c_str(), NULL, 16);
    return (uint32_t)color;
  }
  
  // Fallback-Farben
  if (colorStr.indexOf("Gut") >= 0) return 0x00FF00;
  if (colorStr.indexOf("Mäßig") >= 0) return 0xFFFF00;
  if (colorStr.indexOf("Ungesund") >= 0) return 0xFF0000;
  
  return 0x00FF00; // Default green
}

#endif