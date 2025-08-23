#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "config.h"
#include "SensorManager.h"
#include "TimeUtils.h"

// ===== DISPLAY MANAGER CLASS =====
class DisplayManager {
private:
  U8G2_SH1106_128X64_NONAME_F_HW_I2C& display;
  Adafruit_NeoPixel& leds;
  
  DisplayView currentView = VIEW_OVERVIEW;
  StealthMode stealthMode = STEALTH_OFF;
  bool displayEnabled = true;
  unsigned long stealthTempStartTime = 0;
  
public:
  DisplayManager(U8G2_SH1106_128X64_NONAME_F_HW_I2C& disp, Adafruit_NeoPixel& strip);
  
  void init();
    void updateDisplay(const SensorData& data, float aqi, const String& aqiLevel, bool wifiConnected, bool nodeRedResponding = true);
  void showMessage(const String& message, int duration = 1000);
  
  // View control
  void nextView();
  void toggleStealth();
  void activateStealthTemp();
  
  // Status
  bool isDisplayEnabled() { return displayEnabled; }
  DisplayView getCurrentView() { return currentView; }
  StealthMode getStealthMode() { return stealthMode; }
  
  void resetActivity() { /* Nicht mehr verwendet */ }
  
private:
    void drawOverview(const SensorData& data, float aqi, const String& aqiLevel, bool wifiConnected, bool nodeRedResponding);
    void drawEnvironment(const SensorData& data, bool wifiConnected);
    void drawParticles(const SensorData& data, float aqi, bool wifiConnected);
    void drawGas(const SensorData& data, bool wifiConnected);  // NEU
    void drawSystem(const SensorData& data, bool wifiConnected);
    void drawWiFiIcon(int x, int y, bool connected);
    void drawNodeRedIcon(int x, int y, bool connected);
    void drawConnectionBar(int x, int y, bool wifiConnected, bool nodeRedResponding);
  void updateStealthMode();
  void updateDisplayBrightness();
};

// ===== IMPLEMENTATION =====
DisplayManager::DisplayManager(U8G2_SH1106_128X64_NONAME_F_HW_I2C& disp, Adafruit_NeoPixel& strip) 
  : display(disp), leds(strip) {
}

void DisplayManager::init() {
  DEBUG_PRINTLN("Initializing display...");
  
  // Check if display is available
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() != 0) {
    DEBUG_PRINTLN("Display not available");
    displayEnabled = false;
    return;
  }
  
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(10, 30, "Booting...");
  display.sendBuffer();
  
  updateDisplayBrightness();
  
  DEBUG_PRINTLN("Display initialized successfully");
}

void DisplayManager::updateDisplay(const SensorData& data, float aqi, const String& aqiLevel, bool wifiConnected, bool nodeRedResponding) {
  if (!displayEnabled) {
    return;
  }
  
  updateStealthMode();
  
  // Im Stealth Mode: Display aus
  if (stealthMode == STEALTH_ON) {
    display.clearBuffer();
    display.sendBuffer();
    return;
  }
  
  display.clearBuffer();
  
    switch (currentView) {
      case VIEW_OVERVIEW:
        drawOverview(data, aqi, aqiLevel, wifiConnected, nodeRedResponding);
        break;
      case VIEW_ENVIRONMENT:
        drawEnvironment(data, wifiConnected);
        break;
      case VIEW_PARTICLES:
        drawParticles(data, aqi, wifiConnected);
        break;
      case VIEW_GAS:
        drawGas(data, wifiConnected);
        break;
      case VIEW_SYSTEM:
        drawSystem(data, wifiConnected);
        break;
      default:
        break;
    }
  
  display.sendBuffer();
}

void DisplayManager::drawOverview(const SensorData& data, float aqi, const String& aqiLevel, bool wifiConnected, bool nodeRedResponding) {
  // Header
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "LUFT MONITOR");
  drawConnectionBar(124, 0, wifiConnected, nodeRedResponding);
  
  // AQI - großer Wert
  display.setFont(u8g2_font_ncenB14_tr);
  display.setCursor(0, 28);
  display.printf("AQI: %.0f", aqi);
  
  // AQI Level
  display.setFont(u8g2_font_ncenB08_tr);
  display.setCursor(80, 28);
  display.print(aqiLevel.c_str());
  
  // Hauptwerte - DS18B20 als Haupttemperatur
  display.setFont(u8g2_font_ncenB08_tr);
  display.setCursor(0, 40);
  if (data.ds18b20Available) {
    display.printf("Temp: %.1f°C", data.externalTemp);  // DS18B20 als Haupttemperatur
  } else if (data.bme68xAvailable) {
    display.printf("Temp: %.1f°C", data.temperature);   // Fallback BME680
  } else {
    display.print("Temp: N/A");
  }
  
  display.setCursor(0, 50);
  display.printf("Feucht: %.0f%%", data.humidity);
  
  display.setCursor(0, 60);
  if (data.bme68xAvailable) {
    display.printf("CO2: %.0f ppm", data.co2Equivalent);
    // Genauigkeits-Indikator
    if (data.co2Accuracy >= 2) {
      display.drawStr(120, 60, "*");
    }
  } else {
    display.print("CO2: N/A");
  }
  
  // PM2.5 rechts
  display.setCursor(80, 40);
  display.printf("PM2.5:");
  display.setCursor(80, 50);
  display.printf("%d µg/m³", data.pm2_5);
}

void DisplayManager::drawEnvironment(const SensorData& data, bool wifiConnected) {
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "UMGEBUNG");
  drawWiFiIcon(110, 10, wifiConnected);
  
  // DS18B20 als Haupttemperatur oben
  display.setCursor(0, 25);
  if (data.ds18b20Available) {
    display.printf("Haupt-T: %.1f °C", data.externalTemp);
  } else {
    display.print("Haupt-T: N/A");
  }
  
  // BME68X kompensierte Werte
  display.setCursor(0, 35);
  if (data.bme68xAvailable) {
    display.printf("BME-T: %.1f °C", data.temperature);
    display.setCursor(0, 45);
    display.printf("Feucht: %.1f %%", data.humidity);
    display.setCursor(0, 55);
    display.printf("Druck: %.0f hPa", data.pressure);
    
    if (data.bsecCalibrated) {
      display.drawStr(120, 35, "*");
    }
  } else {
    display.print("BME68X: N/A");
  }
  
  // Temperatur-Differenz anzeigen
  if (data.bme68xAvailable && data.ds18b20Available) {
    float tempDiff = data.externalTemp - data.temperature;
    display.setCursor(0, 62);
    display.printf("Diff: %+.1f °C", tempDiff);
  }
}

void DisplayManager::drawParticles(const SensorData& data, float aqi, bool wifiConnected) {
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "PARTIKEL");
  drawWiFiIcon(110, 10, wifiConnected);
  
  if (data.pms5003Available) {
    display.setCursor(0, 25);
    display.printf("PM1.0: %d µg/m³", data.pm1_0);
    display.setCursor(0, 35);
    display.printf("PM2.5: %d µg/m³", data.pm2_5);
    display.setCursor(0, 45);
    display.printf("PM10:  %d µg/m³", data.pm10);
    
    display.setCursor(0, 60);
    display.printf("AQI: %.0f", aqi);
  } else {
    display.setCursor(0, 30);
    display.print("PMS5003: N/A");
  }
}

void DisplayManager::drawGas(const SensorData& data, bool wifiConnected) {
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "GAS SENSOREN");
  drawWiFiIcon(110, 10, wifiConnected);
  
  if (data.bme68xAvailable) {
    // Kalibrierungs-Status ohne ULP-Mode Text
    display.setCursor(0, 22);
    display.printf("Status: %s", data.bsecCalibrated ? "Kalibriert" : "Lernphase");
    
    // CO2 äquivalent
    display.setCursor(0, 32);
    display.printf("CO2: %.0f ppm", data.co2Equivalent);
    if (data.co2Accuracy >= 2) display.drawStr(120, 32, "*");
    
    // VOC äquivalent
    display.setCursor(0, 42);
    display.printf("VOC: %.1f mg/m³", data.breathVocEquivalent);
    if (data.breathVocAccuracy >= 2) display.drawStr(120, 42, "*");
    
    // IAQ Werte
    display.setCursor(0, 52);
    display.printf("IAQ: %.0f", data.iaq);
    if (data.iaqAccuracy >= 2) display.drawStr(40, 52, "*");
    
    display.setCursor(60, 52);
    display.printf("S-IAQ: %.0f", data.staticIaq);
    if (data.staticIaqAccuracy >= 2) display.drawStr(120, 52, "*");
    
    // Gas Resistance mit besserer Auflösung
    display.setCursor(0, 62);
    if (data.gasResistance > 100000) {
      display.printf("Gas: %.0f kΩ", data.gasResistance / 1000.0);
    } else {
      display.printf("Gas: %.1f kΩ", data.gasResistance / 1000.0);
    }
    
  } else {
    display.setCursor(0, 30);
    display.print("BME68X: N/A");
  }
}

void DisplayManager::drawSystem(const SensorData& data, bool wifiConnected) {
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(0, 10, "SYSTEM");
  drawWiFiIcon(110, 10, wifiConnected);
  
  // Uptime formatiert
  display.setCursor(0, 25);
  uint64_t uptimeSeconds = getUptimeMillis() / 1000;
  unsigned long days = uptimeSeconds / 86400;
  unsigned long hours = (uptimeSeconds % 86400) / 3600;
  unsigned long minutes = (uptimeSeconds % 3600) / 60;
  
  if (days > 0) {
    display.printf("Uptime: %lud %luh", days, hours);
  } else if (hours > 0) {
    display.printf("Uptime: %luh %lum", hours, minutes);
  } else {
    display.printf("Uptime: %lum", minutes);
  }
  
  // WiFi Status
  display.setCursor(0, 35);
  display.printf("WiFi: %s", wifiConnected ? "OK" : "Fehler");
  
  // IP-Adresse oder Offline
  display.setCursor(0, 45);
  if (wifiConnected) {
    display.print(WiFi.localIP().toString().c_str());
  } else {
    display.print("Offline");
  }

  // Sensor Count
  display.setCursor(0, 62);
  display.printf("Sens: %d/3",
    (data.bme68xAvailable ? 1 : 0) + (data.ds18b20Available ? 1 : 0) + (data.pms5003Available ? 1 : 0));
}

void DisplayManager::drawWiFiIcon(int x, int y, bool connected) {
  if (connected) {
    // WiFi Symbol - einfache Bögen
    display.drawPixel(x + 7, y);
    display.drawPixel(x + 6, y - 1);
    display.drawPixel(x + 8, y - 1);
    display.drawPixel(x + 5, y - 2);
    display.drawPixel(x + 9, y - 2);
    display.drawPixel(x + 4, y - 3);
    display.drawPixel(x + 10, y - 3);
  } else {
    // X für keine Verbindung
    display.drawStr(x, y, "X");
  }
}

void DisplayManager::drawNodeRedIcon(int x, int y, bool connected) {
  // Draw two nodes connected by a line when Node-RED responds.
  display.drawCircle(x, y - 3, 2);
  display.drawCircle(x + 6, y - 3, 2);

  if (connected) {
    display.drawLine(x + 2, y - 3, x + 4, y - 3);
  } else {
    display.drawLine(x + 2, y - 5, x + 4, y - 1);
    display.drawLine(x + 2, y - 1, x + 4, y - 5);
  }
}

void DisplayManager::drawConnectionBar(int x, int y, bool wifiConnected, bool nodeRedResponding) {
  const uint8_t segmentWidth = 3;
  const uint8_t segmentHeight = 5;

  if (wifiConnected) {
    display.drawBox(x, y, segmentWidth, segmentHeight);
  } else {
    display.drawFrame(x, y, segmentWidth, segmentHeight);
  }

  if (nodeRedResponding) {
    display.drawBox(x, y + segmentHeight + 1, segmentWidth, segmentHeight);
  } else {
    display.drawFrame(x, y + segmentHeight + 1, segmentWidth, segmentHeight);
  }
}

void DisplayManager::showMessage(const String& message, int duration) {
  if (!displayEnabled) return;
  
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  
  // Zentrieren
  int textWidth = display.getUTF8Width(message.c_str());
  int x = (SCREEN_WIDTH - textWidth) / 2;
  
  display.drawStr(x, 32, message.c_str());
  display.sendBuffer();
  
  if (duration > 0) {
    delay(duration);
  }
}

void DisplayManager::nextView() {
  // View wechseln funktioniert in Normal und Temp Mode
  if (stealthMode != STEALTH_ON) {
    int nextView = (int)currentView + 1;
    if (nextView >= VIEW_COUNT) {
      nextView = 0;
    }
    currentView = (DisplayView)nextView;
    DEBUG_PRINTF("Display view changed to: %d\n", currentView);
  } else {
    // Im reinen Stealth Mode: temporär aktivieren
    activateStealthTemp();
  }
}

void DisplayManager::toggleStealth() {
  if (stealthMode == STEALTH_OFF) {
    stealthMode = STEALTH_ON;
    DEBUG_PRINTLN("Stealth mode ON - display and LEDs dimmed");
  } else {
    stealthMode = STEALTH_OFF;
    DEBUG_PRINTLN("Stealth mode OFF - display and LEDs normal");
  }
  updateDisplayBrightness();
}

void DisplayManager::activateStealthTemp() {
  if (stealthMode == STEALTH_ON) {
    stealthMode = STEALTH_TEMP_ON;
    stealthTempStartTime = millis();
    updateDisplayBrightness();
    DEBUG_PRINTLN("Stealth temporarily activated for 20s");
  }
}

void DisplayManager::updateStealthMode() {
  // Temporärer Stealth Mode Timeout
  if (stealthMode == STEALTH_TEMP_ON) {
    if (millis() - stealthTempStartTime > STEALTH_TEMP_ON_MS) {
      stealthMode = STEALTH_ON;
      updateDisplayBrightness();
      DEBUG_PRINTLN("Stealth temporary mode ended - back to stealth");
    }
  }
}

void DisplayManager::updateDisplayBrightness() {
  if (!displayEnabled) return;
  
  if (stealthMode == STEALTH_ON) {
    display.setContrast(DISPLAY_CONTRAST_STEALTH); // Display aus
  } else {
    display.setContrast(DISPLAY_CONTRAST_NORMAL);  // Display normal
  }
}

#endif