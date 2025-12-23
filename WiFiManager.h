#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "secrets.h"

// ===== WIFI MANAGER CLASS =====
/**
 * @class WiFiManager
 * @brief Manages WiFi connection (no HTTP/Node-RED functionality)
 * 
 * Lightweight WiFi manager for ESP32 - only handles connection,
 * no data transmission to external services.
 */
class WiFiManager {
public:
  /**
   * @brief Constructor
   */
  WiFiManager() {}
  
  /**
   * @brief Connect to WiFi network
   * @return true if connection successful
   */
  bool connect();
  
  /**
   * @brief Check WiFi connection status
   * @return true if WiFi is connected
   */
  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  
  /**
   * @brief Get current IP address
   * @return IP address as String (or empty if not connected)
   */
  String getIPAddress() const {
    if (isConnected()) {
      return WiFi.localIP().toString();
    }
    return String("");
  }
  
  /**
   * @brief Get WiFi RSSI
   * @return Signal strength in dBm
   */
  int getRSSI() const {
    return isConnected() ? WiFi.RSSI() : 0;
  }
};

// ===== IMPLEMENTATION =====
bool WiFiManager::connect() {
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

#endif

