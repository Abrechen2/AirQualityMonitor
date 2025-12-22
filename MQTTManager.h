#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "SensorManager.h"
#include "TimeUtils.h"

// ===== MQTT MANAGER CLASS =====
class MQTTManager {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  
  unsigned long lastPublishTime = 0;
  unsigned long lastReconnectAttempt = 0;
  bool discoveryPublished = false;
  String deviceUniqueId;
  String baseTopic;
  String discoveryPrefix;
  
  // Helper functions
  String getMacAddress();
  void publishDiscoveryConfig();
  void publishSensorDiscovery(const String& sensorName, const String& deviceClass, 
                              const String& unit, const String& icon, 
                              const String& valueTemplate = "");
  void publishAvailability();
  
public:
  MQTTManager();
  
  bool init();
  bool connect();
  void update();
  void publishData(const SensorData& data, float aqi, const String& aqiLevel, 
                  bool wifiConnected, bool nodeRedResponding);
  bool isConnected() { return mqttClient.connected(); }
  
private:
  void reconnect();
  String createDeviceInfo();
};

// ===== IMPLEMENTATION =====
MQTTManager::MQTTManager() : mqttClient(wifiClient) {
  deviceUniqueId = getMacAddress();
  baseTopic = "airqualitymonitor/" + deviceUniqueId;
  discoveryPrefix = "homeassistant/sensor/airqualitymonitor_" + deviceUniqueId;
}

String MQTTManager::getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x%02x%02x%02x%02x%02x", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

String MQTTManager::createDeviceInfo() {
  StaticJsonDocument<256> device;
  device["identifiers"][0] = "airqualitymonitor_" + deviceUniqueId;
  device["name"] = "Air Quality Monitor";
  device["manufacturer"] = "Abrechen2";
  device["model"] = "ESP32 Air Quality Monitor v1.1";
  device["sw_version"] = "1.1.0";
  
  String deviceInfo;
  serializeJson(device, deviceInfo);
  return deviceInfo;
}

void MQTTManager::publishSensorDiscovery(const String& sensorName, const String& deviceClass, 
                                         const String& unit, const String& icon,
                                         const String& valueTemplate) {
  StaticJsonDocument<512> config;
  
  config["name"] = "Air Quality Monitor " + sensorName;
  config["unique_id"] = "airqualitymonitor_" + deviceUniqueId + "_" + sensorName;
  config["state_topic"] = baseTopic + "/" + sensorName + "/state";
  config["availability_topic"] = baseTopic + "/status";
  config["device_class"] = deviceClass;
  config["unit_of_measurement"] = unit;
  config["icon"] = icon;
  
  if (valueTemplate.length() > 0) {
    config["value_template"] = valueTemplate;
  }
  
  // Device info
  StaticJsonDocument<256> device;
  device["identifiers"][0] = "airqualitymonitor_" + deviceUniqueId;
  device["name"] = "Air Quality Monitor";
  device["manufacturer"] = "Abrechen2";
  device["model"] = "ESP32 Air Quality Monitor v1.1";
  device["sw_version"] = "1.1.0";
  config["device"] = device;
  
  String topic = discoveryPrefix + "/" + sensorName + "/config";
  String payload;
  serializeJson(config, payload);
  
  bool published = mqttClient.publish(topic.c_str(), payload.c_str(), true); // retain = true
  if (published) {
    DEBUG_INFO("Published discovery for %s", sensorName.c_str());
  } else {
    DEBUG_WARN("Failed to publish discovery for %s", sensorName.c_str());
  }
}

void MQTTManager::publishDiscoveryConfig() {
  if (discoveryPublished) {
    return;
  }
  
  DEBUG_INFO("Publishing Home Assistant discovery configuration...");
  
  // Environmental sensors
  if (true) { // Always publish, Home Assistant will handle availability
    publishSensorDiscovery("temperature", "temperature", "°C", "mdi:thermometer");
    publishSensorDiscovery("humidity", "humidity", "%", "mdi:water-percent");
    publishSensorDiscovery("pressure", "pressure", "hPa", "mdi:gauge");
    publishSensorDiscovery("external_temperature", "temperature", "°C", "mdi:thermometer");
  }
  
  // Air quality sensors
  publishSensorDiscovery("iaq", "", "", "mdi:air-filter");
  publishSensorDiscovery("iaq_accuracy", "", "", "mdi:check-circle");
  publishSensorDiscovery("co2", "carbon_dioxide", "ppm", "mdi:molecule-co2");
  publishSensorDiscovery("co2_accuracy", "", "", "mdi:check-circle");
  publishSensorDiscovery("voc", "volatile_organic_compounds", "mg/m³", "mdi:air-filter");
  publishSensorDiscovery("voc_accuracy", "", "", "mdi:check-circle");
  publishSensorDiscovery("gas_resistance", "", "Ω", "mdi:gauge");
  
  // Particulate matter sensors
  publishSensorDiscovery("pm1_0", "pm1", "µg/m³", "mdi:air-filter");
  publishSensorDiscovery("pm2_5", "pm25", "µg/m³", "mdi:air-filter");
  publishSensorDiscovery("pm10", "pm10", "µg/m³", "mdi:air-filter");
  
  // Calculated values
  publishSensorDiscovery("aqi", "", "", "mdi:air-filter");
  publishSensorDiscovery("aqi_level", "", "", "mdi:information");
  
  // System sensors
  publishSensorDiscovery("wifi_rssi", "signal_strength", "dBm", "mdi:wifi");
  publishSensorDiscovery("uptime", "", "s", "mdi:timer");
  publishSensorDiscovery("node_red_status", "", "", "mdi:server-network");
  
  discoveryPublished = true;
  DEBUG_INFO("Home Assistant discovery configuration published");
}

void MQTTManager::publishAvailability() {
  String topic = baseTopic + "/status";
  mqttClient.publish(topic.c_str(), "online", true); // retain = true
}

bool MQTTManager::init() {
  DEBUG_INFO("Initializing MQTT Manager...");
  DEBUG_INFO("Device ID: %s", deviceUniqueId.c_str());
  DEBUG_INFO("Base topic: %s", baseTopic.c_str());
  
  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
  mqttClient.setKeepAlive(60);
  
  return true;
}

bool MQTTManager::connect() {
  if (mqttClient.connected()) {
    return true;
  }
  
  DEBUG_INFO("Connecting to MQTT broker %s:%d...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  
  String clientId = "AirQualityMonitor_" + deviceUniqueId;
  bool connected = false;
  
  #ifdef MQTT_USERNAME
    #ifdef MQTT_PASSWORD
      connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
    #else
      connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, "");
    #endif
  #else
    connected = mqttClient.connect(clientId.c_str());
  #endif
  
  if (connected) {
    DEBUG_INFO("MQTT connected successfully");
    publishAvailability();
    
    // Publish discovery config after connection
    if (!discoveryPublished) {
      delay(100); // Small delay to ensure connection is stable
      publishDiscoveryConfig();
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
  
  if (connect()) {
    lastReconnectAttempt = 0;
  }
}

void MQTTManager::update() {
  if (!mqttClient.connected()) {
    reconnect();
  } else {
    mqttClient.loop();
  }
}

void MQTTManager::publishData(const SensorData& data, float aqi, const String& aqiLevel, 
                              bool wifiConnected, bool nodeRedResponding) {
  if (!mqttClient.connected()) {
    return;
  }
  
  unsigned long now = millis();
  if (now - lastPublishTime < MQTT_PUBLISH_INTERVAL) {
    return;
  }
  
  lastPublishTime = now;
  
  // Create JSON payload with all sensor data
  StaticJsonDocument<1024> doc;
  
  // Environmental data
  if (data.bme68xAvailable) {
    doc["temperature"] = data.temperature;
    doc["humidity"] = data.humidity;
    doc["pressure"] = data.pressure;
    doc["iaq"] = data.iaq;
    doc["iaq_accuracy"] = data.iaqAccuracy;
    doc["co2"] = data.co2Equivalent;
    doc["co2_accuracy"] = data.co2Accuracy;
    doc["voc"] = data.breathVocEquivalent;
    doc["voc_accuracy"] = data.breathVocAccuracy;
    doc["gas_resistance"] = data.gasResistance;
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
  
  // Calculated values
  doc["aqi"] = aqi;
  doc["aqi_level"] = aqiLevel;
  
  // System data
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = (uint32_t)(getUptimeMillis() / 1000);
  doc["node_red_status"] = nodeRedResponding ? "online" : "offline";
  
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
  
  // Also publish individual sensor values for compatibility
  // This allows Home Assistant to use value_template in discovery
  if (data.bme68xAvailable) {
    mqttClient.publish((baseTopic + "/temperature/state").c_str(), String(data.temperature).c_str());
    mqttClient.publish((baseTopic + "/humidity/state").c_str(), String(data.humidity).c_str());
    mqttClient.publish((baseTopic + "/pressure/state").c_str(), String(data.pressure).c_str());
    mqttClient.publish((baseTopic + "/iaq/state").c_str(), String(data.iaq).c_str());
    mqttClient.publish((baseTopic + "/co2/state").c_str(), String(data.co2Equivalent).c_str());
    mqttClient.publish((baseTopic + "/voc/state").c_str(), String(data.breathVocEquivalent).c_str());
  }
  
  if (data.ds18b20Available) {
    mqttClient.publish((baseTopic + "/external_temperature/state").c_str(), String(data.externalTemp).c_str());
  }
  
  if (data.pms5003Available) {
    mqttClient.publish((baseTopic + "/pm1_0/state").c_str(), String(data.pm1_0).c_str());
    mqttClient.publish((baseTopic + "/pm2_5/state").c_str(), String(data.pm2_5).c_str());
    mqttClient.publish((baseTopic + "/pm10/state").c_str(), String(data.pm10).c_str());
  }
  
  mqttClient.publish((baseTopic + "/aqi/state").c_str(), String(aqi).c_str());
  mqttClient.publish((baseTopic + "/aqi_level/state").c_str(), aqiLevel.c_str());
  mqttClient.publish((baseTopic + "/wifi_rssi/state").c_str(), String(WiFi.RSSI()).c_str());
  mqttClient.publish((baseTopic + "/uptime/state").c_str(), String((uint32_t)(getUptimeMillis() / 1000)).c_str());
  mqttClient.publish((baseTopic + "/node_red_status/state").c_str(), nodeRedResponding ? "online" : "offline");
}

#endif

