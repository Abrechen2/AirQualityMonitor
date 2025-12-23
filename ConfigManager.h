#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "secrets.h"

// ===== CONFIG MANAGER CLASS =====
/**
 * @class ConfigManager
 * @brief Manages WiFi and MQTT configuration stored in EEPROM
 * 
 * Loads default values from secrets.h and allows runtime configuration via web interface.
 */
class ConfigManager {
private:
  struct ConfigData {
    char wifi_ssid[64];
    char wifi_password[64];
    char mqtt_broker_host[128];
    uint16_t mqtt_broker_port;
    char hostname[64];
    char mqtt_topic[128];
    uint8_t magic;  // Magic byte to detect if config is initialized
  };
  
  static constexpr uint8_t CONFIG_MAGIC = 0xA5;
  ConfigData config;
  bool configLoaded = false;
  
  bool loadFromEEPROM();
  bool saveToEEPROM();
  void loadDefaults();
  bool validateString(const char* str, size_t maxLen, const char* fieldName) const;
  
public:
  /**
   * @brief Constructor - loads config from EEPROM or defaults
   */
  ConfigManager();
  
  /**
   * @brief Initialize EEPROM (must be called after EEPROM.begin())
   */
  bool init();
  
  /**
   * @brief Get WiFi SSID
   */
  const char* getWifiSSID() const { return config.wifi_ssid; }
  
  /**
   * @brief Get WiFi password
   */
  const char* getWifiPassword() const { return config.wifi_password; }
  
  /**
   * @brief Get MQTT broker host
   */
  const char* getMqttBrokerHost() const { return config.mqtt_broker_host; }
  
  /**
   * @brief Get MQTT broker port
   */
  uint16_t getMqttBrokerPort() const { return config.mqtt_broker_port; }
  
  /**
   * @brief Get device hostname
   */
  const char* getHostname() const { return config.hostname; }
  
  /**
   * @brief Get MQTT topic
   */
  const char* getMqttTopic() const { return config.mqtt_topic; }
  
  /**
   * @brief Set WiFi credentials
   * @param ssid WiFi SSID (max 63 chars)
   * @param password WiFi password (max 63 chars)
   * @return true if saved successfully
   */
  bool setWifiConfig(const String& ssid, const String& password);
  
  /**
   * @brief Set MQTT broker configuration
   * @param host MQTT broker hostname/IP (max 127 chars)
   * @param port MQTT broker port
   * @return true if saved successfully
   */
  bool setMqttConfig(const String& host, uint16_t port);
  
  /**
   * @brief Set device hostname
   * @param hostname Device hostname (max 63 chars)
   * @return true if saved successfully
   */
  bool setHostname(const String& hostname);
  
  /**
   * @brief Set MQTT topic
   * @param topic MQTT topic (max 127 chars)
   * @return true if saved successfully
   */
  bool setMqttTopic(const String& topic);
  
  /**
   * @brief Get all config as JSON string
   */
  String toJson() const;
  
  /**
   * @brief Reset config to defaults from secrets.h
   */
  bool resetToDefaults();
};

// ===== IMPLEMENTATION =====
ConfigManager::ConfigManager() {
  loadDefaults();
}

bool ConfigManager::init() {
  if (!configLoaded) {
    if (loadFromEEPROM()) {
      configLoaded = true;
      DEBUG_INFO("Config loaded from EEPROM");
    } else {
      // No valid config in EEPROM or config is corrupt, use defaults and save them
      DEBUG_WARN("No valid config in EEPROM or config is corrupt - loading defaults from secrets.h");
      loadDefaults();  // Reload defaults to ensure clean state
      if (saveToEEPROM()) {
        configLoaded = true;
        DEBUG_INFO("Defaults saved to EEPROM successfully");
      } else {
        DEBUG_ERROR("Failed to save defaults to EEPROM");
        // Still mark as loaded since we have defaults in memory
        configLoaded = true;
      }
    }
  }
  return configLoaded;
}

void ConfigManager::loadDefaults() {
  // Initialize entire struct with zeros first
  memset(&config, 0, sizeof(ConfigData));
  
  // Load defaults from secrets.h
  #ifdef WIFI_SSID
    strncpy(config.wifi_ssid, WIFI_SSID, sizeof(config.wifi_ssid) - 1);
    config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  #else
    config.wifi_ssid[0] = '\0';
  #endif
  
  #ifdef WIFI_PASSWORD
    strncpy(config.wifi_password, WIFI_PASSWORD, sizeof(config.wifi_password) - 1);
    config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  #else
    config.wifi_password[0] = '\0';
  #endif
  
  #ifdef MQTT_BROKER_HOST
    strncpy(config.mqtt_broker_host, MQTT_BROKER_HOST, sizeof(config.mqtt_broker_host) - 1);
    config.mqtt_broker_host[sizeof(config.mqtt_broker_host) - 1] = '\0';
  #else
    config.mqtt_broker_host[0] = '\0';
  #endif
  
  #ifdef MQTT_BROKER_PORT
    config.mqtt_broker_port = MQTT_BROKER_PORT;
  #else
    config.mqtt_broker_port = 1883;
  #endif
  
  #ifdef HOSTNAME
    strncpy(config.hostname, HOSTNAME, sizeof(config.hostname) - 1);
    config.hostname[sizeof(config.hostname) - 1] = '\0';
  #else
    config.hostname[0] = '\0';
  #endif
  
  #ifdef MQTT_TOPIC
    strncpy(config.mqtt_topic, MQTT_TOPIC, sizeof(config.mqtt_topic) - 1);
    config.mqtt_topic[sizeof(config.mqtt_topic) - 1] = '\0';
  #else
    config.mqtt_topic[0] = '\0';
  #endif
  
  config.magic = 0;  // Mark as not saved
  
  DEBUG_INFO("Defaults loaded: WiFi=%s, MQTT=%s:%d, Hostname=%s, Topic=%s",
             config.wifi_ssid, config.mqtt_broker_host, config.mqtt_broker_port,
             config.hostname, config.mqtt_topic);
}

bool ConfigManager::validateString(const char* str, size_t maxLen, const char* fieldName) const {
  if (str == nullptr) {
    DEBUG_WARN("Config validation: %s is null pointer", fieldName);
    return false;
  }
  
  // Check if string is null-terminated within bounds
  bool foundNull = false;
  for (size_t i = 0; i < maxLen; i++) {
    if (str[i] == '\0') {
      foundNull = true;
      // Check for valid ASCII characters (32-126 printable, or 0 for null terminator)
      for (size_t j = 0; j < i; j++) {
        if (str[j] < 32 || str[j] > 126) {
          DEBUG_WARN("Config validation: %s contains invalid character at position %d (0x%02X)", 
                     fieldName, j, (uint8_t)str[j]);
          return false;
        }
      }
      break;
    }
  }
  
  if (!foundNull) {
    DEBUG_WARN("Config validation: %s is not null-terminated", fieldName);
    return false;
  }
  
  // Check if string is not empty (for critical fields)
  if (strlen(str) == 0) {
    DEBUG_WARN("Config validation: %s is empty", fieldName);
    return false;
  }
  
  return true;
}

bool ConfigManager::loadFromEEPROM() {
  if (CONFIG_EEPROM_ADDR + sizeof(ConfigData) > EEPROM.length()) {
    DEBUG_ERROR("Config EEPROM address out of bounds");
    return false;
  }
  
  EEPROM.get(CONFIG_EEPROM_ADDR, config);
  
  // Step 1: Check magic byte first
  if (config.magic != CONFIG_MAGIC) {
    DEBUG_INFO("Config magic byte invalid (0x%02X != 0x%02X)", config.magic, CONFIG_MAGIC);
    return false;
  }
  
  // Step 2: Ensure strings are null-terminated (safety measure)
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  config.mqtt_broker_host[sizeof(config.mqtt_broker_host) - 1] = '\0';
  config.hostname[sizeof(config.hostname) - 1] = '\0';
  config.mqtt_topic[sizeof(config.mqtt_topic) - 1] = '\0';
  
  // Step 3: Validate MQTT port (1-65534, not 0 or 65535 which indicates uninitialized)
  if (config.mqtt_broker_port == 0 || config.mqtt_broker_port >= 65535) {
    DEBUG_WARN("Config loaded but MQTT port is invalid (%d) - using defaults", config.mqtt_broker_port);
    return false;
  }
  
  // Step 4: Validate critical strings (WiFi SSID must not be empty)
  if (!validateString(config.wifi_ssid, sizeof(config.wifi_ssid), "WiFi SSID")) {
    DEBUG_WARN("Config loaded but WiFi SSID is invalid - using defaults");
    return false;
  }
  
  // Step 5: Validate MQTT broker host (must not be empty if port is valid)
  if (!validateString(config.mqtt_broker_host, sizeof(config.mqtt_broker_host), "MQTT broker host")) {
    DEBUG_WARN("Config loaded but MQTT broker host is invalid - using defaults");
    return false;
  }
  
  // Step 6: Validate optional strings (hostname and topic can be empty, but if present must be valid)
  if (strlen(config.hostname) > 0 && !validateString(config.hostname, sizeof(config.hostname), "Hostname")) {
    DEBUG_WARN("Config loaded but hostname is invalid - using defaults");
    return false;
  }
  
  if (strlen(config.mqtt_topic) > 0 && !validateString(config.mqtt_topic, sizeof(config.mqtt_topic), "MQTT topic")) {
    DEBUG_WARN("Config loaded but MQTT topic is invalid - using defaults");
    return false;
  }
  
  // Step 7: Validate WiFi password (can be empty, but if present must be valid)
  if (strlen(config.wifi_password) > 0 && !validateString(config.wifi_password, sizeof(config.wifi_password), "WiFi password")) {
    DEBUG_WARN("Config loaded but WiFi password is invalid - using defaults");
    return false;
  }
  
  // All validations passed - now safe to output debug info
  DEBUG_INFO("Config loaded: WiFi=%s, MQTT=%s:%d, Hostname=%s, Topic=%s", 
             config.wifi_ssid, config.mqtt_broker_host, config.mqtt_broker_port,
             config.hostname, config.mqtt_topic);
  
  return true;
}

bool ConfigManager::saveToEEPROM() {
  if (CONFIG_EEPROM_ADDR + sizeof(ConfigData) > EEPROM.length()) {
    DEBUG_ERROR("Config EEPROM address out of bounds");
    return false;
  }
  
  // Validate config before saving (basic checks)
  if (strlen(config.wifi_ssid) == 0) {
    DEBUG_WARN("Attempting to save config with empty WiFi SSID");
  }
  if (config.mqtt_broker_port == 0 || config.mqtt_broker_port >= 65535) {
    DEBUG_WARN("Attempting to save config with invalid MQTT port (%d)", config.mqtt_broker_port);
  }
  
  config.magic = CONFIG_MAGIC;
  EEPROM.put(CONFIG_EEPROM_ADDR, config);
  
  if (!EEPROM.commit()) {
    DEBUG_ERROR("EEPROM commit failed for config");
    return false;
  }
  
  // Only output debug info if config appears valid
  if (strlen(config.wifi_ssid) > 0 && config.mqtt_broker_port > 0 && config.mqtt_broker_port < 65535) {
    DEBUG_INFO("Config saved to EEPROM: WiFi=%s, MQTT=%s:%d, Hostname=%s, Topic=%s", 
               config.wifi_ssid, config.mqtt_broker_host, config.mqtt_broker_port,
               config.hostname, config.mqtt_topic);
  } else {
    DEBUG_WARN("Config saved to EEPROM but contains invalid values");
  }
  
  return true;
}

bool ConfigManager::setWifiConfig(const String& ssid, const String& password) {
  if (ssid.length() >= sizeof(config.wifi_ssid) || password.length() >= sizeof(config.wifi_password)) {
    DEBUG_ERROR("WiFi config strings too long");
    return false;
  }
  
  strncpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid) - 1);
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
  
  strncpy(config.wifi_password, password.c_str(), sizeof(config.wifi_password) - 1);
  config.wifi_password[sizeof(config.wifi_password) - 1] = '\0';
  
  return saveToEEPROM();
}

bool ConfigManager::setMqttConfig(const String& host, uint16_t port) {
  if (host.length() >= sizeof(config.mqtt_broker_host)) {
    DEBUG_ERROR("MQTT host string too long");
    return false;
  }
  
  strncpy(config.mqtt_broker_host, host.c_str(), sizeof(config.mqtt_broker_host) - 1);
  config.mqtt_broker_host[sizeof(config.mqtt_broker_host) - 1] = '\0';
  
  config.mqtt_broker_port = port;
  
  return saveToEEPROM();
}

bool ConfigManager::setHostname(const String& hostname) {
  if (hostname.length() >= sizeof(config.hostname)) {
    DEBUG_ERROR("Hostname string too long");
    return false;
  }
  
  strncpy(config.hostname, hostname.c_str(), sizeof(config.hostname) - 1);
  config.hostname[sizeof(config.hostname) - 1] = '\0';
  
  return saveToEEPROM();
}

bool ConfigManager::setMqttTopic(const String& topic) {
  if (topic.length() >= sizeof(config.mqtt_topic)) {
    DEBUG_ERROR("MQTT topic string too long");
    return false;
  }
  
  strncpy(config.mqtt_topic, topic.c_str(), sizeof(config.mqtt_topic) - 1);
  config.mqtt_topic[sizeof(config.mqtt_topic) - 1] = '\0';
  
  return saveToEEPROM();
}

String ConfigManager::toJson() const {
  String json = "{";
  json += "\"wifi_ssid\":\"" + String(config.wifi_ssid) + "\",";
  json += "\"wifi_password\":\"" + String(config.wifi_password) + "\",";
  json += "\"mqtt_broker_host\":\"" + String(config.mqtt_broker_host) + "\",";
  json += "\"mqtt_broker_port\":" + String(config.mqtt_broker_port) + ",";
  json += "\"hostname\":\"" + String(config.hostname) + "\",";
  json += "\"mqtt_topic\":\"" + String(config.mqtt_topic) + "\"";
  json += "}";
  return json;
}

bool ConfigManager::resetToDefaults() {
  loadDefaults();
  return saveToEEPROM();
}

#endif

