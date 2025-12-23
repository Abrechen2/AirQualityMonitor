#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ConfigManager.h"

// ===== WIFI MANAGER CLASS =====
/**
 * @class WiFiManager
 * @brief Manages WiFi connection (no HTTP/Node-RED functionality)
 * 
 * Lightweight WiFi manager for ESP32 - only handles connection,
 * no data transmission to external services.
 */
class WiFiManager {
private:
  ConfigManager* configManager;
  
public:
  /**
   * @brief Constructor
   * @param config ConfigManager instance for WiFi credentials
   */
  WiFiManager(ConfigManager* config) : configManager(config) {}
  
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
  if (configManager == nullptr) {
    DEBUG_ERROR("ConfigManager not initialized");
    return false;
  }
  
  const char* ssid = configManager->getWifiSSID();
  const char* password = configManager->getWifiPassword();
  
  // Check if credentials are valid
  if (ssid == nullptr || strlen(ssid) == 0) {
    DEBUG_ERROR("WiFi SSID is empty or not configured");
    return false;
  }
  
  DEBUG_INFO("Connecting to WiFi: %s", ssid);
  if (strlen(password) == 0) {
    DEBUG_WARN("WiFi password is empty - connecting to open network");
  }

  // Disconnect any existing connection
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    attempts++;
    
    // Print status every 2 seconds
    if (attempts % 4 == 0) {
      wl_status_t status = WiFi.status();
      DEBUG_INFO("WiFi status: %d (attempt %d)", status, attempts);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Set hostname if configured, otherwise use MAC-based hostname
    String hostname;
    if (configManager != nullptr && strlen(configManager->getHostname()) > 0) {
      hostname = String(configManager->getHostname());
    } else {
      hostname = "AirQualityMonitor_" + WiFi.macAddress();
      hostname.replace(":", "");
    }
    
    #if defined(ESP32)
      WiFi.setHostname(hostname.c_str());
      DEBUG_INFO("Hostname set to: %s", hostname.c_str());
    #endif
    
    DEBUG_INFO("WiFi connected: %s", WiFi.localIP().toString().c_str());
    DEBUG_INFO("RSSI: %d dBm", WiFi.RSSI());
    DEBUG_INFO("Gateway: %s", WiFi.gatewayIP().toString().c_str());
    DEBUG_INFO("Subnet: %s", WiFi.subnetMask().toString().c_str());
    return true;
  } else {
    wl_status_t status = WiFi.status();
    DEBUG_ERROR("WiFi connection failed after %d attempts", attempts);
    DEBUG_ERROR("Final WiFi status: %d", status);
    
    // Print status code meaning
    switch (status) {
      case WL_NO_SSID_AVAIL:
        DEBUG_ERROR("WiFi error: SSID not found");
        break;
      case WL_CONNECT_FAILED:
        DEBUG_ERROR("WiFi error: Connection failed (wrong password?)");
        break;
      case WL_CONNECTION_LOST:
        DEBUG_ERROR("WiFi error: Connection lost");
        break;
      case WL_DISCONNECTED:
        DEBUG_ERROR("WiFi error: Disconnected");
        break;
      default:
        DEBUG_ERROR("WiFi error: Unknown status code");
        break;
    }
    
    return false;
  }
}

#endif

