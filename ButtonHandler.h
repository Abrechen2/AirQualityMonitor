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
  bool longPressActionTriggered = false;  // Stealth toggle fired while still held

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

  // Handle press start (rising edge of "pressed") — ISR fires on FALLING.
  if (flagCopy) {
    if (currentTime - lastInterruptTime >= BUTTON_DEBOUNCE_MS ||
        currentInterruptCount != lastInterruptCount) {
      selectPressTime = currentTime;
      selectWaitingRelease = true;
      longPressActionTriggered = false;
      lastInterruptTime = currentTime;
      lastInterruptCount = currentInterruptCount;
    }
  }

  if (!selectWaitingRelease) {
    return;
  }

  bool buttonDown = (digitalRead(BUTTON_SELECT_PIN) == LOW);
  unsigned long heldFor = currentTime - selectPressTime;

  if (buttonDown) {
    // Still held — drive the countdown and fire on threshold (don't wait for release).
    if (!longPressActionTriggered) {
      if (heldFor >= BUTTON_LONG_PRESS_MS) {
        // Threshold crossed while still held: fire the stealth toggle NOW
        // so the user sees the effect immediately and can release any time.
        displayManager.hideStealthCountdown();
        handleSelectButtonLong();
        longPressActionTriggered = true;
      } else if (heldFor >= BUTTON_COUNTDOWN_SHOW_AFTER_MS) {
        // Show the 3..2..1 countdown overlay. The DisplayManager
        // rate-limits internally, so calling this every loop is fine.
        uint16_t remaining = (uint16_t)(BUTTON_LONG_PRESS_MS - heldFor);
        displayManager.showStealthCountdown(remaining);
      }
    }
  } else {
    // Released — end the gesture.
    selectWaitingRelease = false;
    displayManager.hideStealthCountdown();

    if (longPressActionTriggered) {
      // Stealth toggle already fired while held — nothing more to do.
      longPressActionTriggered = false;
    } else if (heldFor >= BUTTON_LONG_PRESS_MS) {
      // Safety net: released exactly at / after the threshold without
      // the hold-branch catching it (shouldn't normally happen).
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
