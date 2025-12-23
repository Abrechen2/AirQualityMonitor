// secrets_template.h
//
// SECURITY NOTICE:
// ================
// This file contains sensitive credentials and should NEVER be committed to git.
// Copy this file to "secrets.h" and fill in your actual credentials.
// The .gitignore file ensures secrets.h is not tracked.
//
// RECOMMENDED SECURITY IMPROVEMENTS:
// ==================================
// 1. Use WiFiManager library for runtime credential configuration via web portal
//    (no hardcoded credentials in code)
// 2. Store credentials in EEPROM/SPIFFS with encryption
// 3. Use WPA3 for WiFi if available
// 4. Enable HTTPS for Node-RED communication (requires SSL certificates)
// 5. Implement OTA password protection
//
// For production deployments, consider implementing a secure provisioning system.

#ifndef SECRETS_H
#define SECRETS_H

// ===== WIFI CONFIGURATION =====
// Replace with your actual WiFi credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ===== MQTT CONFIGURATION =====
// MQTT Broker settings for Home Assistant integration
// Replace with your MQTT broker address
#define MQTT_BROKER_HOST "YOUR_MQTT_BROKER_IP"  // e.g., "192.168.1.100" or "mqtt.example.com"
#define MQTT_BROKER_PORT 1883                   // Standard MQTT port (use 8883 for SSL/TLS)

// Optional: MQTT authentication (comment out if not needed)
// #define MQTT_USERNAME "your_mqtt_username"
// #define MQTT_PASSWORD "your_mqtt_password"

// Optional: Custom device ID (leave commented to use MAC address)
// #define MQTT_DEVICE_ID "custom_device_id"

// ===== OTA UPDATE CONFIGURATION =====
// Arduino OTA (Over-The-Air) update password
// Uncomment and set if you want password protection for OTA updates
// The password will be required when uploading via Arduino IDE "Upload via Network"
// #define OTA_PASSWORD "your_ota_password"

// ===== DEVICE CONFIGURATION =====
// Optional: Device hostname (leave commented to use MAC-based hostname)
// Used for OTA updates and mDNS
// #define HOSTNAME "AirQualityMonitor"

// Optional: MQTT topic (leave commented to use MAC-based topic)
// Used as base topic for MQTT messages (e.g., "ENV-Wohn_Dennis/state", "ENV-Wohn_Dennis/status")
// #define MQTT_TOPIC "ENV-Wohn_Dennis"

#endif