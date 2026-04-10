#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConfigManager.h"
#include "SensorManager.h"
#include "DisplayManager.h"
#include "Calculations.h"
#include "TimeUtils.h"
#include "WiFiManager.h"

// ===== MQTT MANAGER CLASS =====
/**
 * @class MQTTManager
 * @brief Manages MQTT communication for Home Assistant integration
 * 
 * Handles MQTT connection, Home Assistant discovery, and sensor data publishing.
 */
class MQTTManager {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  ConfigManager* configManager;
  
  unsigned long lastPublishTime = 0;
  unsigned long lastReconnectAttempt = 0;
  unsigned long lastStatusUpdate = 0;
  unsigned long lastDiscoveryPublish = 0;
  unsigned long lastConnectTime = 0;            // When connect() succeeded (for discovery delay)
  unsigned long nextDiscoveryRetryAt = 0;       // Retry discovery at this time (backoff after abort)
  bool discoveryPublished = false;
  bool discoveryRequestedAfterConnect = false;  // Defer discovery to update() for stable connection
  String deviceUniqueId;
  String baseTopic;
  String discoveryPrefix;
  String binarySensorDiscoveryPrefix;
  String statusTopic;
  
  // Helper functions
  String getMacAddress() const;
  void publishDiscoveryConfig();
  bool publishSensorDiscovery(const String& sensorName, const String& deviceClass, 
                              const String& unit, const String& icon, 
                              const String& valueTemplate = "");
  bool publishBinarySensorDiscovery(const String& sensorName, const String& deviceClass, 
                                    const String& icon, const String& valueTemplate = "");
  void publishAvailability();
  void publishOffline();
  
public:
  /**
   * @brief Constructor - initializes device ID and topics
   * @param config ConfigManager instance for MQTT configuration
   */
  MQTTManager(ConfigManager* config);
  
  /**
   * @brief Initialize MQTT client
   * @return true if initialization successful
   */
  bool init();
  
  /**
   * @brief Connect to MQTT broker
   * @return true if connection successful
   */
  bool connect();
  
  /**
   * @brief Update MQTT client (call in main loop)
   */
  void update();
  
  /**
   * @brief Publish sensor data to MQTT
   * @param data Current sensor data
   * @param aqi Calculated AQI value
   * @param aqiLevel AQI level string
   * @param wifiConnected WiFi connection status
   * @param displayManager Display manager for system data
   */
  void publishData(const SensorData& data, float aqi, const String& aqiLevel, 
                  bool wifiConnected, DisplayManager& displayManager);
  
  /**
   * @brief Check MQTT connection status
   * @return true if connected to broker
   */
  bool isConnected() { return mqttClient.connected(); }
  
private:
  void reconnect();
  String createDeviceInfo() const;
};

// ===== IMPLEMENTATION =====
MQTTManager::MQTTManager(ConfigManager* config) : mqttClient(wifiClient), configManager(config) {
  deviceUniqueId = getMacAddress();
  
  // Use Tasmota-format: tele/{device_id}
  // Priority: Hostname > MQTT Topic > MAC-based fallback
  if (config != nullptr && strlen(config->getHostname()) > 0) {
    baseTopic = "tele/" + String(config->getHostname());
  } else if (config != nullptr && strlen(config->getMqttTopic()) > 0) {
    baseTopic = "tele/" + String(config->getMqttTopic());
  } else {
    baseTopic = "tele/airqualitymonitor_" + deviceUniqueId;
  }
  
  // Status topic for availability
  statusTopic = baseTopic + "/status";
  
  // Discovery prefix always uses deviceUniqueId for compatibility
  discoveryPrefix = "homeassistant/sensor/airqualitymonitor_" + deviceUniqueId;
  binarySensorDiscoveryPrefix = "homeassistant/binary_sensor/airqualitymonitor_" + deviceUniqueId;
}

String MQTTManager::getMacAddress() const {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

String MQTTManager::createDeviceInfo() const {
  StaticJsonDocument<256> device;
  device["identifiers"][0] = "airqualitymonitor_" + deviceUniqueId;
  device["name"] = (configManager && strlen(configManager->getHostname()) > 0)
    ? String(configManager->getHostname())
    : "Air Quality Monitor";
  device["manufacturer"] = "Abrechen2";
  device["model"] = "Air Quality Monitor v1.5";
  device["sw_version"] = "1.5.2";
  
  String deviceInfo;
  serializeJson(device, deviceInfo);
  return deviceInfo;
}

bool MQTTManager::publishSensorDiscovery(const String& sensorName, const String& deviceClass, 
                                         const String& unit, const String& icon,
                                         const String& valueTemplate) {
  // Check if MQTT client is connected
  if (!mqttClient.connected()) {
    DEBUG_WARN("Cannot publish discovery for %s - MQTT not connected", sensorName.c_str());
    return false;
  }
  
  StaticJsonDocument<512> config;
  
  // Name can include hostname for better readability, but unique_id must always be unique
  if (configManager && strlen(configManager->getHostname()) > 0) {
    config["name"] = String(configManager->getHostname()) + " " + sensorName;
  } else {
    config["name"] = sensorName;
  }
  
  // unique_id must always be unique - use full identifier regardless of hostname
  config["unique_id"] = "airqualitymonitor_" + deviceUniqueId + "_" + sensorName;
  config["state_topic"] = baseTopic + "/state";
  config["availability_topic"] = statusTopic;
  
  // Use value_template to extract from JSON state
  if (valueTemplate.length() > 0) {
    config["value_template"] = valueTemplate;
  } else {
    // Default: extract from JSON state
    config["value_template"] = "{{ value_json." + sensorName + " }}";
  }
  
  if (deviceClass.length() > 0) {
    config["device_class"] = deviceClass;
  }
  if (unit.length() > 0) {
    config["unit_of_measurement"] = unit;
  }
  if (icon.length() > 0) {
    config["icon"] = icon;
  }
  
  // Device info - reuse createDeviceInfo() to avoid duplication
  StaticJsonDocument<256> deviceDoc;
  String deviceInfoStr = createDeviceInfo();
  DeserializationError error = deserializeJson(deviceDoc, deviceInfoStr);
  if (!error) {
    config["device"] = deviceDoc;
  } else {
    // Fallback: create device info inline if deserialization fails
    StaticJsonDocument<256> device;
    device["identifiers"][0] = "airqualitymonitor_" + deviceUniqueId;
    device["name"] = (configManager && strlen(configManager->getHostname()) > 0)
      ? String(configManager->getHostname())
      : "Air Quality Monitor";
    device["manufacturer"] = "Abrechen2";
    device["model"] = "Air Quality Monitor v1.5";
    device["sw_version"] = "1.5.2";
    config["device"] = device;
  }
  
  String topic = discoveryPrefix + "/" + sensorName + "/config";
  String payload;
  serializeJson(config, payload);
  
  bool published = mqttClient.publish(topic.c_str(), payload.c_str(), true); // retain = true
  mqttClient.loop();
  delay(10);  // Minimal yield; 100ms was too long — discovery took 3s+, longer than broker keeps connection
  if (published) {
    DEBUG_INFO("Published discovery for %s", sensorName.c_str());
  } else {
    DEBUG_WARN("Failed to publish discovery for %s", sensorName.c_str());
  }
  return published;
}

bool MQTTManager::publishBinarySensorDiscovery(const String& sensorName, const String& deviceClass, 
                                               const String& icon, const String& valueTemplate) {
  // Check if MQTT client is connected
  if (!mqttClient.connected()) {
    DEBUG_WARN("Cannot publish binary sensor discovery for %s - MQTT not connected", sensorName.c_str());
    return false;
  }
  
  StaticJsonDocument<512> config;
  
  // Name can include hostname for better readability, but unique_id must always be unique
  if (configManager && strlen(configManager->getHostname()) > 0) {
    config["name"] = String(configManager->getHostname()) + " " + sensorName;
  } else {
    config["name"] = sensorName;
  }
  
  // unique_id must always be unique - use full identifier regardless of hostname
  config["unique_id"] = "airqualitymonitor_" + deviceUniqueId + "_" + sensorName;
  config["state_topic"] = baseTopic + "/state";
  config["availability_topic"] = statusTopic;
  
  // Binary sensors use payload_on and payload_off
  config["payload_on"] = "1";
  config["payload_off"] = "0";
  
  // Use value_template to extract from JSON state
  if (valueTemplate.length() > 0) {
    config["value_template"] = valueTemplate;
  } else {
    // Default: extract from JSON state
    config["value_template"] = "{{ value_json." + sensorName + " }}";
  }
  
  if (deviceClass.length() > 0) {
    config["device_class"] = deviceClass;
  }
  if (icon.length() > 0) {
    config["icon"] = icon;
  }
  
  // Device info - reuse createDeviceInfo() to avoid duplication
  StaticJsonDocument<256> deviceDoc;
  String deviceInfoStr = createDeviceInfo();
  DeserializationError error = deserializeJson(deviceDoc, deviceInfoStr);
  if (!error) {
    config["device"] = deviceDoc;
  } else {
    // Fallback: create device info inline if deserialization fails
    StaticJsonDocument<256> device;
    device["identifiers"][0] = "airqualitymonitor_" + deviceUniqueId;
    device["name"] = (configManager && strlen(configManager->getHostname()) > 0)
      ? String(configManager->getHostname())
      : "Air Quality Monitor";
    device["manufacturer"] = "Abrechen2";
    device["model"] = "Air Quality Monitor v1.5";
    device["sw_version"] = "1.5.2";
    config["device"] = device;
  }
  
  String topic = binarySensorDiscoveryPrefix + "/" + sensorName + "/config";
  String payload;
  serializeJson(config, payload);
  
  bool published = mqttClient.publish(topic.c_str(), payload.c_str(), true); // retain = true
  mqttClient.loop();
  delay(10);  // Minimal yield — see sensor discovery comment above
  if (published) {
    DEBUG_INFO("Published binary sensor discovery for %s", sensorName.c_str());
  } else {
    DEBUG_WARN("Failed to publish binary sensor discovery for %s", sensorName.c_str());
  }
  return published;
}

void MQTTManager::publishDiscoveryConfig() {
  // CRITICAL: Check if MQTT client is connected before attempting to publish
  if (!mqttClient.connected()) {
    DEBUG_WARN("Cannot publish discovery config - MQTT client not connected");
    return;
  }
  
  unsigned long now = millis();
  
  // Check if we need to republish (periodic republish or first time)
  bool needsRepublish = false;
  if (!discoveryPublished) {
    needsRepublish = true;
    DEBUG_INFO("Discovery not yet published - will publish now");
  } else if (lastDiscoveryPublish > 0 && (now - lastDiscoveryPublish) >= DISCOVERY_REPUBLISH_INTERVAL) {
    needsRepublish = true;
    DEBUG_INFO("Periodic discovery republish triggered (60 minutes elapsed)");
  }
  
  if (!needsRepublish) {
    DEBUG_INFO("Discovery republish not needed (discoveryPublished=%d, lastDiscoveryPublish=%lu, elapsed=%lu)", 
               discoveryPublished, lastDiscoveryPublish, lastDiscoveryPublish > 0 ? (now - lastDiscoveryPublish) : 0);
    return;
  }
  
  DEBUG_INFO("Publishing Home Assistant discovery configuration... (MQTT connected: %d)", mqttClient.connected());
  
  // Ensure MQTT loop is processed before publishing
  mqttClient.loop();
  
  bool allOk = true;
  int successCount = 0;
  int failCount = 0;
  bool discoveryAborted = false;
  
  #define DISCOVERY_CHECK_CONNECTED() do { \
    mqttClient.loop(); \
    if (!mqttClient.connected()) { \
      DEBUG_WARN("MQTT disconnected during discovery - stopping. Will retry on next connect."); \
      discoveryAborted = true; \
      goto discovery_end; \
    } \
  } while(0)
  
  // Environmental sensors
  if (true) { // Always publish, Home Assistant will handle availability
    DISCOVERY_CHECK_CONNECTED();
    if (publishSensorDiscovery("temperature", "temperature", "°C", "mdi:thermometer")) successCount++; else failCount++;
    if (publishSensorDiscovery("humidity", "humidity", "%", "mdi:water-percent")) successCount++; else failCount++;
    if (publishSensorDiscovery("pressure", "pressure", "hPa", "mdi:gauge")) successCount++; else failCount++;
    if (publishSensorDiscovery("external_temperature", "temperature", "°C", "mdi:thermometer")) successCount++; else failCount++;
  }
  
  // Air quality sensors
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("iaq", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("iaq_accuracy", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("static_iaq", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("static_iaq_accuracy", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("co2", "carbon_dioxide", "ppm", "mdi:molecule-co2")) successCount++; else failCount++;
  if (publishSensorDiscovery("co2_accuracy", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("voc", "volatile_organic_compounds", "mg/m³", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("voc_accuracy", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("gas_resistance", "", "Ω", "mdi:gauge")) successCount++; else failCount++;
  
  // Particulate matter sensors
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("pm1_0", "pm1", "µg/m³", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("pm2_5", "pm25", "µg/m³", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("pm10", "pm10", "µg/m³", "mdi:air-filter")) successCount++; else failCount++;
  
  // Calculated comfort values
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("dew_point", "temperature", "°C", "mdi:thermometer-water")) successCount++; else failCount++;
  if (publishSensorDiscovery("heat_index", "temperature", "°C", "mdi:thermometer")) successCount++; else failCount++;
  if (publishSensorDiscovery("absolute_humidity", "", "g/m³", "mdi:water")) successCount++; else failCount++;
  if (publishSensorDiscovery("comfort_index", "", "", "mdi:sofa")) successCount++; else failCount++;
  
  // AQI values
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("aqi_index", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("aqi_category", "", "", "mdi:information")) successCount++; else failCount++;
  if (publishSensorDiscovery("pm2_5_aqi", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("pm10_aqi", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("iaq_aqi", "", "", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("bsec_calibrated", "", "", "mdi:check-circle")) successCount++; else failCount++;
  
  // System sensors
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("wifi_rssi", "signal_strength", "dBm", "mdi:wifi")) successCount++; else failCount++;
  if (publishSensorDiscovery("wifi_connected", "", "", "mdi:wifi")) successCount++; else failCount++;
  if (publishSensorDiscovery("mqtt_connected", "", "", "mdi:server-network")) successCount++; else failCount++;
  if (publishSensorDiscovery("uptime", "", "s", "mdi:timer")) successCount++; else failCount++;
  if (publishSensorDiscovery("free_heap", "", "bytes", "mdi:memory")) successCount++; else failCount++;
  if (publishSensorDiscovery("ip_address", "", "", "mdi:ip-network")) successCount++; else failCount++;
  if (publishSensorDiscovery("stealth_mode", "", "", "mdi:eye-off")) successCount++; else failCount++;
  if (publishSensorDiscovery("display_enabled", "", "", "mdi:monitor")) successCount++; else failCount++;
  if (publishSensorDiscovery("current_view", "", "", "mdi:view-dashboard")) successCount++; else failCount++;
  if (publishSensorDiscovery("sensors_available_count", "", "", "mdi:counter")) successCount++; else failCount++;
  if (publishSensorDiscovery("sensor_bme68x_available", "", "", "mdi:chip")) successCount++; else failCount++;
  if (publishSensorDiscovery("sensor_ds18b20_available", "", "", "mdi:chip")) successCount++; else failCount++;
  if (publishSensorDiscovery("sensor_pms5003_available", "", "", "mdi:chip")) successCount++; else failCount++;
  if (publishSensorDiscovery("sensor_reliable", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("bme68x_stable", "", "", "mdi:check-circle")) successCount++; else failCount++;
  if (publishSensorDiscovery("bme68x_runin_complete", "", "", "mdi:check-circle")) successCount++; else failCount++;
  
  // Alert binary sensors
  DISCOVERY_CHECK_CONNECTED();
  if (publishBinarySensorDiscovery("alert_aqi", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("alert_co2", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("alert_pm25", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("alert_tvoc", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("alert_humidity_low", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("alert_humidity_high", "problem", "mdi:alert")) successCount++; else failCount++;
  if (publishBinarySensorDiscovery("ventilation_needed", "", "mdi:fan")) successCount++; else failCount++;
  
  // Additional sensor values
  DISCOVERY_CHECK_CONNECTED();
  if (publishSensorDiscovery("tvoc_ppb", "", "ppb", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("tvoc_mgm3", "", "mg/m³", "mdi:air-filter")) successCount++; else failCount++;
  if (publishSensorDiscovery("aqi_color_code", "", "", "mdi:palette")) successCount++; else failCount++;
  
discovery_end:
  #undef DISCOVERY_CHECK_CONNECTED
  // If discovery aborted due to disconnect, retry after 60 s (backoff to avoid 2s loop)
  if (discoveryAborted) {
    nextDiscoveryRetryAt = now + 60000;
  }
  // Final loop to ensure all messages are sent
  mqttClient.loop();
  delay(100); // Small delay to allow messages to be sent
  mqttClient.loop();
  
  allOk = (failCount == 0 && !discoveryAborted);
  
  if (allOk) {
    discoveryPublished = true;
    lastDiscoveryPublish = now;
    DEBUG_INFO("Home Assistant discovery configuration published successfully: %d sensors published, 0 failed", successCount);
  } else {
    DEBUG_ERROR("Discovery publish failed: %d sensors published, %d failed - will retry on next connect", 
                successCount, failCount);
    DEBUG_ERROR("Discovery published flag will remain false to force retry");
    // Keep discoveryPublished = false so it will retry on next connect
  }
}

void MQTTManager::publishAvailability() {
  if (mqttClient.connected()) {
    mqttClient.publish(statusTopic.c_str(), "online", true); // retain = true
    lastStatusUpdate = millis();
    DEBUG_INFO("Published availability: online");
  }
}

void MQTTManager::publishOffline() {
  if (mqttClient.connected()) {
    mqttClient.publish(statusTopic.c_str(), "offline", true); // retain = true
    DEBUG_INFO("Published availability: offline");
  }
}

bool MQTTManager::init() {
  if (configManager == nullptr) {
    DEBUG_ERROR("ConfigManager not initialized");
    return false;
  }
  
  DEBUG_INFO("Initializing MQTT Manager...");
  // Re-read MAC here (after WiFi init) in case constructor ran before WiFi was ready
  deviceUniqueId = getMacAddress();
  DEBUG_INFO("Device ID (MAC): %s", deviceUniqueId.c_str());
  DEBUG_INFO("Base topic: %s", baseTopic.c_str());

  mqttClient.setServer(configManager->getMqttBrokerHost(), configManager->getMqttBrokerPort());
  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  mqttClient.setKeepAlive(60);
  
  return true;
}

bool MQTTManager::connect() {
  if (mqttClient.connected()) {
    return true;
  }
  
  DEBUG_INFO("Connecting to MQTT broker %s:%d...", configManager->getMqttBrokerHost(), configManager->getMqttBrokerPort());
  
  // Use hostname as client ID so multiple devices with identical MAC (e.g. cloned ESP32s)
  // get unique IDs. Hostname is stored per-device in EEPROM via ConfigManager.
  // Fall back to MAC-based ID if no hostname is configured.
  String clientId;
  if (configManager && strlen(configManager->getHostname()) > 0) {
    clientId = "AirQualityMonitor_" + String(configManager->getHostname());
  } else {
    clientId = "AirQualityMonitor_" + deviceUniqueId;
  }
  DEBUG_INFO("MQTT client ID: %s", clientId.c_str());
  bool connected = false;
  
  // Set Last Will and Testament (LWT) - sends "offline" if connection is lost unexpectedly
  // Will topic: same as status topic, will message: "offline", will retain: true, will QoS: 1
  String willTopic = statusTopic;
  const char* willMessage = "offline";
  uint8_t willQos = 1;
  bool willRetain = true;
  
  #ifdef MQTT_USERNAME
    #ifdef MQTT_PASSWORD
      connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, 
                                     willTopic.c_str(), willQos, willRetain, willMessage);
    #else
      connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, "", 
                                     willTopic.c_str(), willQos, willRetain, willMessage);
    #endif
  #else
    connected = mqttClient.connect(clientId.c_str(), willTopic.c_str(), willQos, willRetain, willMessage);
  #endif
  
  if (connected) {
    DEBUG_INFO("MQTT connected successfully with LWT");
    lastConnectTime = millis();
    // Process any immediate broker message (e.g. DISCONNECT sent before our first loop())
    mqttClient.loop();
    if (!mqttClient.connected()) {
      DEBUG_WARN("Broker closed connection immediately after CONNACK! State: %d", mqttClient.state());
      return false;
    }
    publishAvailability();
    // Subscribe to command topic so the broker has a reason to keep the connection alive
    // and we can later receive remote commands (e.g. display control).
    String cmdTopic = baseTopic + "/cmd/#";
    mqttClient.subscribe(cmdTopic.c_str());
    DEBUG_INFO("Subscribed to %s", cmdTopic.c_str());
    // Schedule discovery whenever we (re)connect and it has not yet succeeded.
    // Guard with !discoveryRequestedAfterConnect to avoid duplicate scheduling.
    if (!discoveryPublished && !discoveryRequestedAfterConnect) {
      discoveryRequestedAfterConnect = true;
      DEBUG_INFO("Discovery scheduled after 500ms stabilization");
    }
    return true;
  } else {
    DEBUG_ERROR("MQTT connection failed, state: %d", mqttClient.state());
    return false;
  }
}

void MQTTManager::reconnect() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL) {
    return;
  }
  
  lastReconnectAttempt = now;
  
  // Do not reset discoveryPublished here - avoids immediate discovery on reconnect and 2s online/offline loop
  
  // Publish offline before reconnecting (if we were connected before)
  // Note: This might not always work if connection is already lost, but LWT will handle it
  if (mqttClient.state() != MQTT_CONNECTED && lastStatusUpdate > 0) {
    // Try to publish offline, but don't wait if it fails
    publishOffline();
  }
  
  connect();
  // Do NOT reset lastReconnectAttempt to 0 — that would bypass the 5-second throttle
  // on the very next disconnect, causing the rapid-reconnect loop seen in the serial log.
}

void MQTTManager::update() {
  if (!mqttClient.connected()) {
    int state = mqttClient.state();
    if (lastConnectTime > 0) {
      unsigned long aliveMs = millis() - lastConnectTime;
      DEBUG_WARN("MQTT disconnected after %lums! PubSubClient state: %d", aliveMs, state);
      // State codes: -4=TIMEOUT -3=CONN_LOST -2=CONN_FAILED -1=DISCONNECTED
      //              1=BAD_PROTOCOL 2=BAD_CLIENT_ID 3=UNAVAILABLE 4=BAD_CREDENTIALS 5=UNAUTHORIZED
      lastConnectTime = 0; // Prevent repeated logging until next successful connect
    }
    reconnect();
  } else {
    mqttClient.loop();
    
    unsigned long now = millis();
    
    // Run deferred discovery from connect() after stabilization delay (500ms).
    // Reduced from 2000ms: the broker closes connections within 76-1354ms; waiting
    // longer meant discovery never had a chance to run.
    if (discoveryRequestedAfterConnect && mqttClient.connected()) {
      const unsigned long kDiscoveryStabilizationMs = 500;
      if (now - lastConnectTime >= kDiscoveryStabilizationMs) {
        discoveryRequestedAfterConnect = false;
        DEBUG_INFO("Triggering discovery publish (500ms stabilization elapsed)");
        publishDiscoveryConfig();
      }
    }
    // Retry discovery with 60 s backoff when previous run aborted due to disconnect
    if (!discoveryPublished && nextDiscoveryRetryAt > 0 && now >= nextDiscoveryRetryAt && mqttClient.connected()) {
      nextDiscoveryRetryAt = now + 60000;
      DEBUG_INFO("Triggering discovery retry (backoff)");
      publishDiscoveryConfig();
    }
    
    // Send keep-alive status update every 60 seconds
    if (now - lastStatusUpdate >= 60000) { // 60 seconds
      publishAvailability();
    }
    
    // Periodically republish discovery config (every 60 minutes)
    if (discoveryPublished && lastDiscoveryPublish > 0 && 
        (now - lastDiscoveryPublish) >= DISCOVERY_REPUBLISH_INTERVAL) {
      DEBUG_INFO("Triggering periodic discovery republish");
      publishDiscoveryConfig();
    }
  }
}

void MQTTManager::publishData(const SensorData& data, float aqi, const String& aqiLevel, 
                              bool wifiConnected, DisplayManager& displayManager) {
  if (!mqttClient.connected()) {
    return;
  }
  
  unsigned long now = millis();
  if (now - lastPublishTime < MQTT_PUBLISH_INTERVAL) {
    return;
  }
  
  lastPublishTime = now;
  
  // Calculate all derived values
  float dewPoint = 0.0f;
  float heatIndex = 0.0f;
  float absoluteHumidity = 0.0f;
  uint8_t comfortIndex = 0;
  AlertFlags alerts = {0};
  uint8_t aqiCategory = getAQICategory(aqi);
  uint16_t pm25_aqi = 0;
  uint16_t pm10_aqi = 0;
  uint16_t iaq_aqi = 0;
  
  if (data.bme68xAvailable) {
    dewPoint = calculateDewPoint(data.temperature, data.humidity);
    heatIndex = calculateHeatIndex(data.temperature, data.humidity);
    absoluteHumidity = calculateAbsoluteHumidity(data.temperature, data.humidity);
    comfortIndex = calculateComfortIndex(data.temperature, data.humidity, heatIndex);
    alerts = calculateAlertFlags(data, aqi);
    iaq_aqi = calculateIAQtoAQI(data.iaq);
  }
  
  if (data.pms5003Available) {
    pm25_aqi = calculatePM25AQI(data.pm2_5);
    pm10_aqi = calculatePM10AQI(data.pm10);
  }
  
  // Create JSON payload with all sensor data (increased size for all fields)
  StaticJsonDocument<1536> doc;
  
  // Direct sensor data - use short names matching discovery config
  if (data.bme68xAvailable) {
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["pressure"] = data.pressure;
    doc["iaq"] = data.iaq;
    doc["static_iaq"] = data.staticIaq;
    doc["iaq_accuracy"] = data.iaqAccuracy;
    doc["static_iaq_accuracy"] = data.staticIaqAccuracy;
    doc["co2"] = data.co2Equivalent;
    doc["co2_accuracy"] = data.co2Accuracy;
    doc["voc"] = data.breathVocEquivalent;
    doc["voc_accuracy"] = data.breathVocAccuracy;
    doc["gas_resistance"] = data.gasResistance;
    doc["bsec_calibrated"] = data.bsecCalibrated ? 1 : 0;
    doc["tvoc_ppb"] = (uint16_t)(data.breathVocEquivalent * 1000.0f);
    doc["tvoc_mgm3"] = data.breathVocEquivalent;
  }
  
  if (data.ds18b20Available) {
    doc["external_temperature"] = data.externalTemp;
  }
  
  // Particulate matter
  if (data.pms5003Available) {
    doc["pm1_0"] = data.pm1_0;
    doc["pm2_5"] = data.pm2_5;
    doc["pm10"] = data.pm10;
  }
  
  // Calculated comfort values
  if (data.bme68xAvailable) {
    doc["dew_point"] = dewPoint;
    doc["heat_index"] = heatIndex;
    doc["absolute_humidity"] = absoluteHumidity;
    doc["comfort_index"] = comfortIndex;
  }
  
  // AQI values
  doc["aqi_index"] = aqi;
  doc["aqi_category"] = aqiCategory;
  if (data.pms5003Available) {
    doc["pm2_5_aqi"] = pm25_aqi;
    doc["pm10_aqi"] = pm10_aqi;
  }
  if (data.bme68xAvailable) {
    doc["iaq_aqi"] = iaq_aqi;
  }
  
  // System status - use short names matching discovery
  doc["sensor_reliable"] = ((data.bme68xAvailable && data.iaqAccuracy > 0) || 
                            (data.pms5003Available && data.pm2_5 > 0) || 
                            data.ds18b20Available) ? 1 : 0;
  doc["bme68x_stable"] = (data.bme68xAvailable && data.iaqAccuracy >= 2) ? 1 : 0;
  doc["bme68x_runin_complete"] = (data.bme68xAvailable && data.iaqAccuracy >= 3) ? 1 : 0;
  doc["sensors_available_count"] = (data.bme68xAvailable ? 1 : 0) + 
                                    (data.ds18b20Available ? 1 : 0) + 
                                    (data.pms5003Available ? 1 : 0);
  doc["sensor_bme68x_available"] = data.bme68xAvailable ? 1 : 0;
  doc["sensor_ds18b20_available"] = data.ds18b20Available ? 1 : 0;
  doc["sensor_pms5003_available"] = data.pms5003Available ? 1 : 0;
  doc["wifi_rssi"] = wifiConnected ? WiFi.RSSI() : 0;
  doc["wifi_connected"] = wifiConnected ? 1 : 0;
  doc["mqtt_connected"] = mqttClient.connected() ? 1 : 0;
  doc["stealth_mode"] = (displayManager.getStealthMode() == STEALTH_ON) ? 1 : 0;
  doc["display_enabled"] = displayManager.isDisplayEnabled() ? 1 : 0;
  doc["current_view"] = (uint8_t)displayManager.getCurrentView();
  doc["uptime"] = (uint32_t)(getUptimeMillis() / 1000);
  doc["free_heap"] = ESP.getFreeHeap();
  if (wifiConnected) {
    doc["ip_address"] = WiFi.localIP().toString();
  } else {
    doc["ip_address"] = "";
  }
  
  // Alert flags - always include, even if 0
  doc["alert_aqi"] = (data.bme68xAvailable && alerts.alert_aqi) ? 1 : 0;
  doc["alert_co2"] = (data.bme68xAvailable && alerts.alert_co2) ? 1 : 0;
  doc["alert_pm25"] = (data.pms5003Available && alerts.alert_pm25) ? 1 : 0;
  doc["alert_tvoc"] = (data.bme68xAvailable && alerts.alert_tvoc) ? 1 : 0;
  doc["alert_humidity_low"] = (data.bme68xAvailable && alerts.alert_humidity_low) ? 1 : 0;
  doc["alert_humidity_high"] = (data.bme68xAvailable && alerts.alert_humidity_high) ? 1 : 0;
  doc["ventilation_needed"] = (data.bme68xAvailable && alerts.ventilation_needed) ? 1 : 0;
  
  // AQI color code
  uint32_t aqiColor = calculateAQIColor(aqi);
  char colorStr[8];
  snprintf(colorStr, sizeof(colorStr), "#%06X", aqiColor);
  doc["aqi_color_code"] = colorStr;
  
  // Publish as single JSON message to main state topic
  String payload;
  serializeJson(doc, payload);
  
  String stateTopic = baseTopic + "/state";
  bool published = mqttClient.publish(stateTopic.c_str(), payload.c_str());
  
  if (published) {
    DEBUG_INFO("Published sensor data to MQTT (%d bytes)", payload.length());
  } else {
    DEBUG_WARN("Failed to publish sensor data to MQTT");
  }
}

#endif

