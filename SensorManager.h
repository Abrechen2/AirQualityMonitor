#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "bsec.h"
#include "PMS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "config.h"

// ===== SENSOR DATA STRUCTURE =====
struct SensorData {
  // BME68X/BSEC data
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
  float gasResistance = 0.0;
  float iaq = 0.0;
  float staticIaq = 0.0;
  float co2Equivalent = 400.0;
  float breathVocEquivalent = 0.0;
  uint8_t iaqAccuracy = 0;
  uint8_t staticIaqAccuracy = 0;
  uint8_t co2Accuracy = 0;
  uint8_t breathVocAccuracy = 0;
  bool bsecCalibrated = false;
  bool bme68xAvailable = false;
  
  // DS18B20 data
  float externalTemp = 0.0;
  bool ds18b20Available = false;
  
  // PMS5003 data
  uint16_t pm1_0 = 0;
  uint16_t pm2_5 = 0;
  uint16_t pm10 = 0;
  bool pms5003Available = false;
};

// ===== SENSOR MANAGER CLASS =====
class SensorManager {
private:
  Bsec& bme68x;
  PMS& pms5003;
  PMS::DATA pmsData;
  
  OneWire oneWire;
  DallasTemperature ds18b20;
  
  SensorData currentData;
  unsigned long lastSensorRead = 0;
  unsigned long lastStateTime = 0;
  
  // Sensor corrections
  float tempCorrection = DEFAULT_TEMP_CORRECTION;
  float humidityCorrection = DEFAULT_HUMIDITY_CORRECTION;

public:
  SensorManager(Bsec& bsec, PMS& pms);
  
  bool init();
  bool update();
  SensorData getData() { return currentData; }
  
  void setTempCorrection(float correction) { tempCorrection = correction; }
  void setHumidityCorrection(float correction) { humidityCorrection = correction; }

private:
  bool scanI2CDevice(uint8_t address);
  bool initBME68X(uint8_t address);
  void configureBsecSensors();
  bool initDS18B20();
  bool initPMS5003();
  
  bool readBME68X();
  bool readDS18B20();
  bool readPMS5003();
  
  void saveBsecState();
  void loadBsecState();
  void printSensorStatus();
};

// ===== IMPLEMENTATION =====
SensorManager::SensorManager(Bsec& bsec, PMS& pms) 
  : bme68x(bsec), pms5003(pms), oneWire(DS18B20_PIN), ds18b20(&oneWire) {}

bool SensorManager::init() {
  DEBUG_INFO("Initializing sensors...");
  
  // EEPROM for BSEC state
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 10);
  
  bool success = true;
  
  // Initialize BME68X
  uint8_t bmeAddress = 0;
  if (scanI2CDevice(BME68X_I2C_ADDR_HIGH)) {
    bmeAddress = BME68X_I2C_ADDR_HIGH;
  } else if (scanI2CDevice(BME68X_I2C_ADDR_LOW)) {
    bmeAddress = BME68X_I2C_ADDR_LOW;
  }

  if (bmeAddress != 0) {
    success &= initBME68X(bmeAddress);
  } else {
    DEBUG_ERROR("BME68X not found");
    currentData.bme68xAvailable = false;
  }
  
  // Initialize DS18B20
  success &= initDS18B20();
  
  // Initialize PMS5003
  success &= initPMS5003();
  
  printSensorStatus();
  return success;
}

bool SensorManager::update() {
  if (millis() - lastSensorRead < SENSOR_READ_INTERVAL) {
    return false;
  }
  
  bool dataUpdated = false;
  
  // Read BME68X
  if (currentData.bme68xAvailable) {
    dataUpdated |= readBME68X();
  }
  
  // Read DS18B20 (less frequently)
  static unsigned long lastDS18B20Read = 0;
  if (currentData.ds18b20Available && (millis() - lastDS18B20Read > 10000)) {
    dataUpdated |= readDS18B20();
    lastDS18B20Read = millis();
  }
  
  // Read PMS5003
  if (currentData.pms5003Available) {
    dataUpdated |= readPMS5003();
  }
  
  // Save BSEC state every 6h
  if (currentData.bme68xAvailable && (millis() - lastStateTime > BSEC_STATE_SAVE_INTERVAL)) {
    saveBsecState();
    lastStateTime = millis();
  }
  
  if (dataUpdated) {
    lastSensorRead = millis();
  }
  
  return dataUpdated;
}

bool SensorManager::scanI2CDevice(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

bool SensorManager::initBME68X(uint8_t address) {
  DEBUG_INFO("Initializing BME68X with BSEC at 0x%02X...", address);

  try {
    bme68x.begin(address, Wire);

    if (bme68x.bsecStatus != BSEC_OK || bme68x.bme68xStatus != BME68X_OK) {
      DEBUG_ERROR("BME68X init failed at 0x%02X - BSEC: %d, BME68X: %d", address, bme68x.bsecStatus, bme68x.bme68xStatus);
      currentData.bme68xAvailable = false;
      return false;
    }

    // Mark sensor as available so state can be loaded
    currentData.bme68xAvailable = true;

    // Configure BSEC sensors
    configureBsecSensors();

    // Load stored state
    loadBsecState();
    DEBUG_INFO("BME68X with BSEC initialized successfully");
    return true;

  } catch (...) {
    DEBUG_ERROR("BME68X exception during init");
    currentData.bme68xAvailable = false;
    return false;
  }
}

void SensorManager::configureBsecSensors() {
  // Enable all available BSEC outputs (ULP mode for better response)
  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };

  // ULP mode: 0.33 Hz for gas + 0.1 Hz for temp/hum - better compromise
  // Available modes: BSEC_SAMPLE_RATE_ULP, BSEC_SAMPLE_RATE_LP, BSEC_SAMPLE_RATE_CONT
  bme68x.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_ULP);
  
  DEBUG_INFO("BSEC ULP Mode configured - Status: %d", bme68x.bsecStatus);
  DEBUG_INFO("Response Time: ~1.4s (LP Mode), Update Rate: 0.33Hz, Power: ~0.1mA");
}

bool SensorManager::initDS18B20() {
  DEBUG_INFO("Initializing DS18B20...");
  
  try {
    ds18b20.begin();
    
    int deviceCount = ds18b20.getDeviceCount();
    if (deviceCount == 0) {
      DEBUG_WARN("DS18B20 not found");
      currentData.ds18b20Available = false;
      return false;
    }
    
    ds18b20.setResolution(12);
    currentData.ds18b20Available = true;
    DEBUG_INFO("DS18B20 found %d device(s)", deviceCount);
    return true;
    
  } catch (...) {
    DEBUG_ERROR("DS18B20 exception during init");
    currentData.ds18b20Available = false;
    return false;
  }
}

bool SensorManager::initPMS5003() {
  DEBUG_INFO("Initializing PMS5003...");
  
  try {
    pms5003.passiveMode();
    pms5003.wakeUp(); 
    delay(1000);
    
    currentData.pms5003Available = true;
    DEBUG_INFO("PMS5003 initialized successfully");
    return true;
    
  } catch (...) {
    DEBUG_ERROR("PMS5003 exception during init");
    currentData.pms5003Available = false;
    return false;
  }
}

bool SensorManager::readBME68X() {
  if (!bme68x.run()) {
    return false;
  }
  
  // Compensated values from BSEC
  currentData.temperature = bme68x.temperature + tempCorrection;
  currentData.humidity = bme68x.humidity + humidityCorrection;
  currentData.pressure = bme68x.pressure / 100.0; // hPa
  currentData.gasResistance = bme68x.gasResistance;
  
  // BSEC gas algorithm outputs
  currentData.iaq = bme68x.iaq;
  currentData.iaqAccuracy = bme68x.iaqAccuracy;
  currentData.staticIaq = bme68x.staticIaq;
  currentData.staticIaqAccuracy = bme68x.staticIaqAccuracy;
  currentData.co2Equivalent = bme68x.co2Equivalent;
  currentData.co2Accuracy = bme68x.co2Accuracy;
  currentData.breathVocEquivalent = bme68x.breathVocEquivalent;
  currentData.breathVocAccuracy = bme68x.breathVocAccuracy;
  
  // Calibration status
  currentData.bsecCalibrated = (currentData.iaqAccuracy >= 2);
  
  return true;
}

bool SensorManager::readDS18B20() {
  ds18b20.requestTemperatures();
  delay(750); // Conversion time for 12-bit
  
  float temp = ds18b20.getTempCByIndex(0);
  
  if (temp == DEVICE_DISCONNECTED_C) {
    DEBUG_WARN("DS18B20 read failed");
    return false;
  }
  
  currentData.externalTemp = temp;
  return true;
}

bool SensorManager::readPMS5003() {
  pms5003.wakeUp();
  delay(2000); // PMS5003 wake up time optimised

  for (int i = 0; i < 3; i++) {
    pms5003.requestRead();
    if (pms5003.readUntil(pmsData, 1000)) {
      currentData.pm1_0 = pmsData.PM_AE_UG_1_0;
      currentData.pm2_5 = pmsData.PM_AE_UG_2_5;
      currentData.pm10 = pmsData.PM_AE_UG_10_0;
      pms5003.sleep();
      return true;
    }
  }

  DEBUG_WARN("PMS5003 read failed after 3 attempts");
  pms5003.sleep();
  return false;
}

void SensorManager::saveBsecState() {
  if (!currentData.bme68xAvailable || !currentData.bsecCalibrated) return;
  
  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
  uint8_t workBuffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};
  uint32_t serializedStateLength = 0;
  
  bsec_library_return_t status = bsec_get_state(0, bsecState, sizeof(bsecState), workBuffer, sizeof(workBuffer), &serializedStateLength);
  
  if (status == BSEC_OK) {
    if (serializedStateLength > BSEC_MAX_STATE_BLOB_SIZE - 4) {
      DEBUG_ERROR("BSEC state too large: %d bytes", serializedStateLength);
      return;
    }

    if (BSEC_BASELINE_EEPROM_ADDR + 4 + serializedStateLength > EEPROM.length()) {
      DEBUG_ERROR("EEPROM overflow prevented");
      return;
    }

    // Store length
    EEPROM.put(BSEC_BASELINE_EEPROM_ADDR, serializedStateLength);

    // Save state
    for (uint32_t i = 0; i < serializedStateLength; i++) {
      EEPROM.write(BSEC_BASELINE_EEPROM_ADDR + 4 + i, bsecState[i]);
    }

    EEPROM.commit();
    DEBUG_INFO("BSEC state saved (%d bytes)", serializedStateLength);
  } else {
    DEBUG_WARN("BSEC state save failed: %d", status);
  }
}

void SensorManager::loadBsecState() {
  if (!currentData.bme68xAvailable) return;
  
  uint32_t serializedStateLength = 0;
  EEPROM.get(BSEC_BASELINE_EEPROM_ADDR, serializedStateLength);
  
  // Plausibility check
  if (serializedStateLength == 0 || serializedStateLength > BSEC_MAX_STATE_BLOB_SIZE) {
    DEBUG_WARN("No valid BSEC state found - starting fresh");
    return;
  }
  
  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
  uint8_t workBuffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};
  
  // Load state
  for (uint32_t i = 0; i < serializedStateLength; i++) {
    bsecState[i] = EEPROM.read(BSEC_BASELINE_EEPROM_ADDR + 4 + i);
  }
  
  bsec_library_return_t status = bsec_set_state(bsecState, serializedStateLength, workBuffer, sizeof(workBuffer));

  if (status == BSEC_OK) {
    DEBUG_INFO("BSEC state loaded (%d bytes)", serializedStateLength);
    currentData.bsecCalibrated = true;
  } else {
    DEBUG_WARN("BSEC state load failed: %d", status);
  }
}

void SensorManager::printSensorStatus() {
  DEBUG_INFO("=== Sensor Status ===");
  DEBUG_INFO("BME68X: %s", currentData.bme68xAvailable ? "OK" : "NOT FOUND");
  DEBUG_INFO("DS18B20: %s", currentData.ds18b20Available ? "OK" : "NOT FOUND");
  DEBUG_INFO("PMS5003: %s", currentData.pms5003Available ? "OK" : "NOT FOUND");
  
  if (currentData.bme68xAvailable) {
    DEBUG_INFO("BSEC Status: %d", bme68x.bsecStatus);
    DEBUG_INFO("BME68X Status: %d", bme68x.bme68xStatus);
  }
}

#endif