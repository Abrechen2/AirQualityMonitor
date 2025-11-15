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

// ===== NODE-RED ENDPOINTS =====
// Replace with your Node-RED server address
// SECURITY: Use HTTPS in production: "https://YOUR_SERVER:1880/..."
#define NODERED_SEND_URL "http://YOUR_SERVER:1880/sensor-data"
#define NODERED_AQI_URL "http://YOUR_SERVER:1880/calculate-aqi"

#endif