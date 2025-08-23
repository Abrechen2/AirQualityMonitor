#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "DisplayManager.h"

// ===== LED MANAGER CLASS =====
class LEDManager {
private:
  Adafruit_NeoPixel& strip;
  DisplayManager& displayManager;
  
public:
  LEDManager(Adafruit_NeoPixel& ledStrip, DisplayManager& display);
  
  void init();
  void updateLEDs(uint32_t aqiColor);
  
private:
  uint8_t getCurrentBrightness();
};

// ===== IMPLEMENTATION =====
LEDManager::LEDManager(Adafruit_NeoPixel& ledStrip, DisplayManager& display) 
  : strip(ledStrip), displayManager(display) {
}

void LEDManager::init() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS_NORMAL);
  strip.show();
  DEBUG_INFO("LEDs initialized");
}

void LEDManager::updateLEDs(uint32_t aqiColor) {
  StealthMode stealthMode = displayManager.getStealthMode();
  
  // Brightness based on stealth mode
  uint8_t brightness = getCurrentBrightness();
  strip.setBrightness(brightness);
  
  if (stealthMode == STEALTH_ON) {
    // Stealth mode: only one LED with AQI color
    strip.clear();
    strip.setPixelColor(0, aqiColor);  // first LED
  } else {
    // Normal/temp mode: all LEDs with AQI color
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, aqiColor);
    }
  }
  
  strip.show();
}

uint8_t LEDManager::getCurrentBrightness() {
  StealthMode stealthMode = displayManager.getStealthMode();
  
  switch (stealthMode) {
    case STEALTH_ON:
      return LED_BRIGHTNESS_STEALTH;
    case STEALTH_OFF:
    case STEALTH_TEMP_ON:
    default:
      return LED_BRIGHTNESS_NORMAL;
  }
}

#endif