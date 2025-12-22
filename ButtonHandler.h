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
  static volatile uint32_t interruptCount;  // ISR-safe counter instead of millis()
  static portMUX_TYPE selectMux;
  static void IRAM_ATTR selectISR();

  unsigned long selectPressTime = 0;
  unsigned long lastInterruptTime = 0;
  uint32_t lastInterruptCount = 0;
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
volatile uint32_t ButtonHandler::interruptCount = 0;
portMUX_TYPE ButtonHandler::selectMux = portMUX_INITIALIZER_UNLOCKED;

// ===== ISR DEFINITION =====
// ISR-safe: Only set flag, no millis() call
void IRAM_ATTR ButtonHandler::selectISR() {
  portENTER_CRITICAL_ISR(&selectMux);
  interruptCount++;
  selectFlag = true;
  portEXIT_CRITICAL_ISR(&selectMux);
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

  portENTER_CRITICAL(&selectMux);
  bool flagCopy = selectFlag;
  uint32_t currentInterruptCount = interruptCount;
  selectFlag = false;
  portEXIT_CRITICAL(&selectMux);

  // Debounce: Only process if enough time has passed since last interrupt
  if (flagCopy) {
    if (currentTime - lastInterruptTime >= BUTTON_DEBOUNCE_MS || 
        currentInterruptCount != lastInterruptCount) {
      selectPressTime = currentTime;
      selectWaitingRelease = true;
      lastInterruptTime = currentTime;
      lastInterruptCount = currentInterruptCount;
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
