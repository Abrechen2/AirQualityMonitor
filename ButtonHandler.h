#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "DisplayManager.h"

// ===== BUTTON HANDLER CLASS =====
class ButtonHandler {
private:
  DisplayManager& displayManager;

  static volatile bool selectFlag;
  static void IRAM_ATTR selectISR();

  unsigned long lastSelectEvent = 0;
  unsigned long selectPressTime = 0;
  bool selectWaitingRelease = false;

public:
  ButtonHandler(DisplayManager& display);
  void init();
  void update();

private:
  void handleSelectButtonShort();
  void handleSelectButtonLong();
};

// ===== STATIC MEMBER INITIALIZATION =====
volatile bool ButtonHandler::selectFlag = false;

// ===== ISR DEFINITION =====
void IRAM_ATTR ButtonHandler::selectISR() {
  selectFlag = true;
}

// ===== IMPLEMENTATION =====
ButtonHandler::ButtonHandler(DisplayManager& display) : displayManager(display) {}

void ButtonHandler::init() {
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT_PIN), selectISR, FALLING);
  
  DEBUG_INFO("Select button initialized with interrupt");
}

void ButtonHandler::update() {
  unsigned long currentTime = millis();

  // Select button handling
  if (selectFlag) {
    selectFlag = false;
    if (currentTime - lastSelectEvent > BUTTON_DEBOUNCE_MS) {
      lastSelectEvent = currentTime;
      selectPressTime = currentTime;
      selectWaitingRelease = true;
    }
  }

  if (selectWaitingRelease && digitalRead(BUTTON_SELECT_PIN) == HIGH) {
    selectWaitingRelease = false;
    if ((currentTime - selectPressTime) > BUTTON_LONG_PRESS_MS) {
      handleSelectButtonLong();
    } else {
      handleSelectButtonShort();
    }
  }
}

void ButtonHandler::handleSelectButtonShort() {
  StealthMode currentStealth = displayManager.getStealthMode();

  if (currentStealth == STEALTH_OFF) {
    // Normal mode: switch view
    displayManager.nextView();
    DEBUG_INFO("Select short: view switched");
  } else {
    // Stealth mode: activate temporarily
    displayManager.activateStealthTemp();
    DEBUG_INFO("Select short: stealth temp activated");
  }
}

void ButtonHandler::handleSelectButtonLong() {
  // Stealth Mode Toggle
  displayManager.toggleStealth();
  DEBUG_INFO("Select long: stealth mode toggled");
}

#endif