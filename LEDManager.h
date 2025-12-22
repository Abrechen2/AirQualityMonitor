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
  
  // Transition state
  uint32_t currentColor = 0x00FF00;  // Current displayed color
  uint32_t targetColor = 0x00FF00;   // Target color
  uint8_t transitionStep = 0;         // Current transition step (0 = complete)
  unsigned long lastTransitionTime = 0;
  
public:
  LEDManager(Adafruit_NeoPixel& ledStrip, DisplayManager& display);
  
  void init();
  void updateLEDs(uint32_t aqiColor);
  void updateLEDsWithTransition(uint32_t targetColor);
  
private:
  uint8_t getCurrentBrightness();
  void interpolateColor();
  uint32_t lerpColor(uint32_t color1, uint32_t color2, float t);
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
  // Immediate update (for stealth mode or when transition not needed)
  StealthMode stealthMode = displayManager.getStealthMode();
  
  // Brightness based on stealth mode
  uint8_t brightness = getCurrentBrightness();
  strip.setBrightness(brightness);
  
  currentColor = aqiColor;
  targetColor = aqiColor;
  transitionStep = LED_TRANSITION_STEPS; // Mark as complete
  
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

void LEDManager::updateLEDsWithTransition(uint32_t newTargetColor) {
  StealthMode stealthMode = displayManager.getStealthMode();
  
  // In stealth mode, use immediate update
  if (stealthMode == STEALTH_ON) {
    updateLEDs(newTargetColor);
    return;
  }
  
  // Update target color if it changed
  if (newTargetColor != targetColor) {
    targetColor = newTargetColor;
    if (transitionStep >= LED_TRANSITION_STEPS) {
      // Start new transition
      transitionStep = 0;
      lastTransitionTime = millis();
    }
  }
  
  // Perform transition step if needed
  if (transitionStep < LED_TRANSITION_STEPS) {
    unsigned long now = millis();
    if (now - lastTransitionTime >= LED_TRANSITION_INTERVAL_MS) {
      transitionStep++;
      lastTransitionTime = now;
      interpolateColor();
    }
  } else {
    // Transition complete, ensure we're at target
    currentColor = targetColor;
  }
  
  // Brightness based on stealth mode
  uint8_t brightness = getCurrentBrightness();
  strip.setBrightness(brightness);
  
  // Apply current color to all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, currentColor);
  }
  
  strip.show();
}

void LEDManager::interpolateColor() {
  float t = (float)transitionStep / (float)LED_TRANSITION_STEPS;
  currentColor = lerpColor(currentColor, targetColor, t);
}

uint32_t LEDManager::lerpColor(uint32_t color1, uint32_t color2, float t) {
  // Clamp t to [0, 1]
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  
  // Extract RGB components
  uint8_t r1 = (color1 >> 16) & 0xFF;
  uint8_t g1 = (color1 >> 8) & 0xFF;
  uint8_t b1 = color1 & 0xFF;
  
  uint8_t r2 = (color2 >> 16) & 0xFF;
  uint8_t g2 = (color2 >> 8) & 0xFF;
  uint8_t b2 = color2 & 0xFF;
  
  // Linear interpolation
  uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
  uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
  uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
  
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
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