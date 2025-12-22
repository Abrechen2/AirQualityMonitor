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

// ===== SENSOR STATE MACHINES =====
enum DS18B20State {
  DS18B20_IDLE,
  DS18B20_REQUESTED,
  DS18B20_READING
};

enum PMS5003State {
  PMS5003_SLEEPING,
  PMS5003_WAKING,
  PMS5003_READING,
  PMS5003_RETRY
};

// ===== SENSOR MANAGER CLASS =====
/**
 * @class SensorManager
 * @brief Manages all sensor operations including BME68X, DS18B20, and PMS5003
 * 
 * This class handles initialization, reading, and state management for all sensors.
 * It uses asynchronous state machines for non-blocking sensor operations.
 */
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

  // Async state machines
  DS18B20State ds18State = DS18B20_IDLE;
  unsigned long ds18RequestTime = 0;
  unsigned long lastDS18B20Read = 0;

  PMS5003State pmsState = PMS5003_SLEEPING;
  unsigned long pmsStateTime = 0;
  uint8_t pmsRetryCount = 0;

public:
  /**
   * @brief Constructor
   * @param bsec Reference to BSEC sensor instance
   * @param pms Reference to PMS5003 sensor instance
   */
  SensorManager(Bsec& bsec, PMS& pms);
  
  /**
   * @brief Initialize all sensors
   * @return true if at least one sensor initialized successfully
   */
  bool init();
  
  /**
   * @brief Update sensor readings (non-blocking)
   * @return true if new data is available
   */
  bool update();
  
  /**
   * @brief Get current sensor data
   * @return Const reference to current sensor data
   */
  const SensorData& getData() const { return currentData; }
  
  /**
   * @brief Set temperature correction offset
   * @param correction Temperature offset in degrees Celsius
   */
  void setTempCorrection(float correction) { tempCorrection = correction; }
  
  /**
   * @brief Set humidity correction offset
   * @param correction Humidity offset in percent
   */
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

  bool saveBsecState();
  bool loadBsecState();
  void resetBsecCalibration();
  void printSensorStatus();
  
  // Validation functions
  bool validateSensorData(const SensorData& data);
  bool isValidTemperature(float temp);
  bool isValidHumidity(float humidity);
  bool isValidPressure(float pressure);
  bool isValidIAQ(float iaq);
  bool isValidCO2(float co2);
  bool isValidVOC(float voc);
  bool isValidPM(uint16_t pm);
};

// ===== IMPLEMENTATION =====
SensorManager::SensorManager(Bsec& bsec, PMS& pms) 
  : bme68x(bsec), pms5003(pms), oneWire(DS18B20_PIN), ds18b20(&oneWire) {}

bool SensorManager::init() {
  DEBUG_INFO("Initializing sensors...");

  // EEPROM for BSEC state (needs 221 bytes minimum)
  if (!EEPROM.begin(512)) {  // Use 512 bytes to be safe
    DEBUG_ERROR("EEPROM initialization failed!");
  } else {
    DEBUG_INFO("EEPROM initialized (512 bytes)");
  }

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
  bool dataUpdated = false;

  // Read BME68X - BSEC must be called continuously in every loop
  if (currentData.bme68xAvailable) {
    if (bme68x.run()) {
      // New data available from BSEC
      dataUpdated |= readBME68X();
    } else {
      // Check for errors
      if (bme68x.bsecStatus != BSEC_OK) {
        DEBUG_ERROR("BSEC error: %d", bme68x.bsecStatus);
      }
      if (bme68x.bme68xStatus != BME68X_OK) {
        DEBUG_ERROR("BME68X sensor error: %d", bme68x.bme68xStatus);
      }
    }
  }

  // Read DS18B20 asynchronously
  if (currentData.ds18b20Available) {
    if (ds18State == DS18B20_IDLE && (millis() - lastDS18B20Read > DS18B20_READ_INTERVAL_MS)) {
      // Start new read cycle
      readDS18B20();  // Starts the state machine
    } else {
      // Continue state machine
      if (readDS18B20()) {
        dataUpdated = true;
        lastDS18B20Read = millis();
      }
    }
  }

  // Read PMS5003 asynchronously (every SENSOR_READ_INTERVAL)
  if (currentData.pms5003Available) {
    if (pmsState == PMS5003_SLEEPING && (millis() - lastSensorRead >= SENSOR_READ_INTERVAL)) {
      // Start new read cycle
      readPMS5003();  // Starts the state machine
    } else {
      // Continue state machine
      if (readPMS5003()) {
        dataUpdated = true;
      }
    }
  }

  // Save BSEC state every 6h
  if (currentData.bme68xAvailable && (millis() - lastStateTime > BSEC_STATE_SAVE_INTERVAL)) {
    if (saveBsecState()) {
      lastStateTime = millis();
    } else {
      DEBUG_WARN("BSEC state save failed - will retry in next interval");
    }
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

    DEBUG_INFO("After begin() - BSEC status: %d, BME68X status: %d", bme68x.bsecStatus, bme68x.bme68xStatus);

    if (bme68x.bsecStatus != BSEC_OK) {
      DEBUG_ERROR("BSEC init failed with status: %d", bme68x.bsecStatus);
      if (bme68x.bsecStatus < BSEC_OK) {
        DEBUG_ERROR("BSEC Error Code (negative = error)");
      }
      currentData.bme68xAvailable = false;
      return false;
    }

    if (bme68x.bme68xStatus != BME68X_OK) {
      DEBUG_ERROR("BME68X sensor init failed with status: %d", bme68x.bme68xStatus);
      currentData.bme68xAvailable = false;
      return false;
    }

    // Mark sensor as available so state can be loaded
    currentData.bme68xAvailable = true;
    DEBUG_INFO("BME68X communication established successfully");

    // Configure BSEC sensors
    configureBsecSensors();

    DEBUG_INFO("After configureBsecSensors() - BSEC status: %d", bme68x.bsecStatus);

    if (bme68x.bsecStatus != BSEC_OK) {
      DEBUG_ERROR("BSEC configuration failed with status: %d", bme68x.bsecStatus);
      currentData.bme68xAvailable = false;
      return false;
    }

    // Reset calibration because mode changed from ULP to LP
    // IMPORTANT: Comment out the next line after first successful run!
    //resetBsecCalibration();

    // Load stored state (non-critical if it fails)
    if (loadBsecState()) {
      DEBUG_INFO("Previous BSEC calibration state loaded");
    } else {
      DEBUG_INFO("Starting fresh calibration (no saved state)");
    }

    DEBUG_INFO("BME68X with BSEC initialized successfully");
    DEBUG_INFO("Sensor will provide first readings after warm-up (~3s in LP mode)");
    return true;

  } catch (...) {
    DEBUG_ERROR("BME68X exception during init");
    currentData.bme68xAvailable = false;
    return false;
  }
}

void SensorManager::configureBsecSensors() {
  // Configure BSEC outputs with LP mode for better CO2/VOC performance
  // LP mode (3s interval) is required for reliable CO2 and VOC readings
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

  // LP mode: 0.33 Hz (3 second interval) - required for CO2/VOC outputs
  // ULP mode does not provide reliable CO2 and VOC equivalent values
  // Available modes: BSEC_SAMPLE_RATE_ULP, BSEC_SAMPLE_RATE_LP, BSEC_SAMPLE_RATE_CONT
  bme68x.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);

  DEBUG_INFO("BSEC LP Mode configured - Status: %d", bme68x.bsecStatus);
  DEBUG_INFO("Response Time: ~3s, Update Rate: 0.33Hz, Power: ~0.3mA");
  DEBUG_INFO("CO2 and VOC outputs enabled with reliable sample rate");
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
  // Don't call run() again - already called in update()
  // Just read the latest values from BSEC

  // Compensated values from BSEC with validation
  float rawTemp = bme68x.temperature + tempCorrection;
  float rawHumidity = bme68x.humidity + humidityCorrection;
  float rawPressure = bme68x.pressure / 100.0; // hPa

  // Validate and clamp sensor values
  currentData.temperature = isValidTemperature(rawTemp) ? rawTemp : 
                           (rawTemp < TEMP_MIN ? TEMP_MIN : TEMP_MAX);
  currentData.humidity = isValidHumidity(rawHumidity) ? rawHumidity : 
                         (rawHumidity < HUMIDITY_MIN ? HUMIDITY_MIN : HUMIDITY_MAX);
  currentData.pressure = isValidPressure(rawPressure) ? rawPressure : 
                        (rawPressure < PRESSURE_MIN ? PRESSURE_MIN : PRESSURE_MAX);
  currentData.gasResistance = (bme68x.gasResistance > 0) ? bme68x.gasResistance : 0;

  // BSEC gas algorithm outputs with validation
  float rawIAQ = bme68x.iaq;
  float rawCO2 = bme68x.co2Equivalent;
  float rawVOC = bme68x.breathVocEquivalent;

  currentData.iaq = isValidIAQ(rawIAQ) ? rawIAQ : 0.0f;
  currentData.iaqAccuracy = bme68x.iaqAccuracy;
  currentData.staticIaq = isValidIAQ(bme68x.staticIaq) ? bme68x.staticIaq : 0.0f;
  currentData.staticIaqAccuracy = bme68x.staticIaqAccuracy;
  currentData.co2Equivalent = isValidCO2(rawCO2) ? rawCO2 : CO2_MIN;
  currentData.co2Accuracy = bme68x.co2Accuracy;
  currentData.breathVocEquivalent = isValidVOC(rawVOC) ? rawVOC : VOC_MIN;
  currentData.breathVocAccuracy = bme68x.breathVocAccuracy;

  // Calibration status
  currentData.bsecCalibrated = (currentData.iaqAccuracy >= 2);

  // Debug output for first few readings
  static uint8_t debugCount = 0;
  if (debugCount < 5) {
    DEBUG_INFO("BME68X Read - Temp: %.1f°C, Hum: %.1f%%, Press: %.1f hPa, Gas: %.0f Ohm",
               currentData.temperature, currentData.humidity, currentData.pressure, currentData.gasResistance);
    debugCount++;
  }

  return true;
}

bool SensorManager::readDS18B20() {
  // Non-blocking state machine for DS18B20
  switch (ds18State) {
    case DS18B20_IDLE:
      // Start temperature conversion
      ds18b20.requestTemperatures();
      ds18RequestTime = millis();
      ds18State = DS18B20_REQUESTED;
      return false;

    case DS18B20_REQUESTED:
      // Wait for conversion time
      if (millis() - ds18RequestTime >= DS18B20_CONVERSION_TIME_MS) {
        ds18State = DS18B20_READING;
      }
      return false;

    case DS18B20_READING:
      // Read temperature
      float temp = ds18b20.getTempCByIndex(0);
      ds18State = DS18B20_IDLE;

      if (temp == DEVICE_DISCONNECTED_C) {
        DEBUG_WARN("DS18B20 read failed");
        return false;
      }

      // Validate temperature before storing
      if (isValidTemperature(temp)) {
        currentData.externalTemp = temp;
      } else {
        DEBUG_WARN("DS18B20 invalid temperature: %.2f", temp);
        currentData.externalTemp = (temp < TEMP_MIN ? TEMP_MIN : TEMP_MAX);
      }
      return true;
  }
  return false;
}

bool SensorManager::readPMS5003() {
  // Non-blocking state machine for PMS5003
  switch (pmsState) {
    case PMS5003_SLEEPING:
      // Wake up sensor
      pms5003.wakeUp();
      pmsStateTime = millis();
      pmsRetryCount = 0;
      pmsState = PMS5003_WAKING;
      return false;

    case PMS5003_WAKING:
      // Wait for wake up time
      if (millis() - pmsStateTime >= PMS_WAKE_TIME_MS) {
        pms5003.requestRead();
        pmsStateTime = millis();
        pmsState = PMS5003_READING;
      }
      return false;

    case PMS5003_READING:
      // Try to read data with timeout
      if (pms5003.readUntil(pmsData, PMS_READ_TIMEOUT_MS / 20)) {  // Non-blocking check
        // Validate PM values before storing
        if (isValidPM(pmsData.PM_AE_UG_1_0) && isValidPM(pmsData.PM_AE_UG_2_5) && isValidPM(pmsData.PM_AE_UG_10_0)) {
          currentData.pm1_0 = pmsData.PM_AE_UG_1_0;
          currentData.pm2_5 = pmsData.PM_AE_UG_2_5;
          currentData.pm10 = pmsData.PM_AE_UG_10_0;
        } else {
          DEBUG_WARN("PMS5003 invalid PM values: PM1.0=%d, PM2.5=%d, PM10=%d",
                     pmsData.PM_AE_UG_1_0, pmsData.PM_AE_UG_2_5, pmsData.PM_AE_UG_10_0);
          // Use last valid values or zero
          currentData.pm1_0 = 0;
          currentData.pm2_5 = 0;
          currentData.pm10 = 0;
        }
        pms5003.sleep();
        pmsState = PMS5003_SLEEPING;
        return true;
      }

      // Check timeout
      if (millis() - pmsStateTime >= PMS_READ_TIMEOUT_MS) {
        pmsRetryCount++;
        if (pmsRetryCount >= PMS_RETRY_COUNT) {
          // Failed after 2 attempts
          DEBUG_WARN("PMS5003 read failed after %d attempts", pmsRetryCount);
          pms5003.sleep();
          pmsState = PMS5003_SLEEPING;
          return false;
        } else {
          // Retry
          pms5003.requestRead();
          pmsStateTime = millis();
          pmsState = PMS5003_READING;
        }
      }
      return false;

    case PMS5003_RETRY:
      // This state is not used anymore, merged into READING
      pmsState = PMS5003_SLEEPING;
      return false;
  }
  return false;
}

bool SensorManager::saveBsecState() {
  if (!currentData.bme68xAvailable || !currentData.bsecCalibrated) {
    return false;
  }

  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
  uint8_t workBuffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};
  uint32_t serializedStateLength = 0;

  bsec_library_return_t status = bsec_get_state(0, bsecState, sizeof(bsecState), workBuffer, sizeof(workBuffer), &serializedStateLength);

  if (status != BSEC_OK) {
    DEBUG_ERROR("BSEC state get failed: %d", status);
    return false;
  }

  if (serializedStateLength > BSEC_MAX_STATE_BLOB_SIZE - 4) {
    DEBUG_ERROR("BSEC state too large: %d bytes", serializedStateLength);
    return false;
  }

  if (serializedStateLength > BSEC_MAX_STATE_BLOB_SIZE) {
    DEBUG_ERROR("BSEC state exceeds buffer size");
    return false;
  }

  if (BSEC_BASELINE_EEPROM_ADDR + 4 + serializedStateLength > EEPROM.length()) {
    DEBUG_ERROR("EEPROM overflow prevented");
    return false;
  }

  // Store length
  EEPROM.put(BSEC_BASELINE_EEPROM_ADDR, serializedStateLength);

  // Save state
  for (uint32_t i = 0; i < serializedStateLength; i++) {
    EEPROM.write(BSEC_BASELINE_EEPROM_ADDR + 4 + i, bsecState[i]);
  }

  if (!EEPROM.commit()) {
    DEBUG_ERROR("EEPROM commit failed");
    return false;
  }

  DEBUG_INFO("BSEC state saved (%d bytes)", serializedStateLength);
  return true;
}

bool SensorManager::loadBsecState() {
  if (!currentData.bme68xAvailable) {
    DEBUG_WARN("BME68X not available - cannot load BSEC state");
    return false;
  }

  uint32_t serializedStateLength = 0;
  EEPROM.get(BSEC_BASELINE_EEPROM_ADDR, serializedStateLength);

  // Plausibility check
  if (serializedStateLength == 0) {
    DEBUG_INFO("No BSEC state found in EEPROM - starting fresh calibration");
    return false;
  }

  if (serializedStateLength > BSEC_MAX_STATE_BLOB_SIZE) {
    DEBUG_ERROR("BSEC state length invalid: %d bytes (max: %d) (ErrorCode: %d)", 
                serializedStateLength, BSEC_MAX_STATE_BLOB_SIZE, ERROR_INVALID_DATA);
    return false;
  }

  // Check EEPROM bounds
  uint32_t requiredSize = BSEC_BASELINE_EEPROM_ADDR + 4 + serializedStateLength;
  if (requiredSize > EEPROM.length()) {
    DEBUG_ERROR("EEPROM read would overflow: need %d bytes, have %d (ErrorCode: %d)", 
                requiredSize, EEPROM.length(), ERROR_EEPROM_FAILED);
    return false;
  }

  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
  uint8_t workBuffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};

  // Load state with error checking
  for (uint32_t i = 0; i < serializedStateLength; i++) {
    uint8_t byte = EEPROM.read(BSEC_BASELINE_EEPROM_ADDR + 4 + i);
    if (byte == 0xFF && i == 0) {
      // EEPROM might be uninitialized
      DEBUG_WARN("EEPROM appears uninitialized at offset %d", i);
    }
    bsecState[i] = byte;
  }

  bsec_library_return_t status = bsec_set_state(bsecState, serializedStateLength, workBuffer, sizeof(workBuffer));

  if (status == BSEC_OK) {
    DEBUG_INFO("BSEC state loaded successfully (%d bytes)", serializedStateLength);
    currentData.bsecCalibrated = true;
    return true;
  } else {
    DEBUG_ERROR("BSEC state load failed: %d (ErrorCode: %d)", status, ERROR_SENSOR_READ_FAILED);
    return false;
  }
}

void SensorManager::resetBsecCalibration() {
  DEBUG_WARN("=== RESETTING BSEC CALIBRATION ===");
  DEBUG_INFO("Clearing EEPROM saved state...");

  // Write zeros to EEPROM to invalidate saved state
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }

  if (EEPROM.commit()) {
    DEBUG_INFO("EEPROM cleared successfully - BSEC will start fresh calibration");
    DEBUG_INFO("Calibration timeline:");
    DEBUG_INFO("  - First 5 min: accuracy = 0 (warming up)");
    DEBUG_INFO("  - 5-20 min: accuracy = 1 (initial calibration)");
    DEBUG_INFO("  - 20+ min: accuracy = 2-3 (calibrated)");
  } else {
    DEBUG_ERROR("EEPROM clear failed!");
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

// Validation functions
bool SensorManager::isValidTemperature(float temp) {
  return (temp >= TEMP_MIN && temp <= TEMP_MAX) && !isnan(temp) && !isinf(temp);
}

bool SensorManager::isValidHumidity(float humidity) {
  return (humidity >= HUMIDITY_MIN && humidity <= HUMIDITY_MAX) && !isnan(humidity) && !isinf(humidity);
}

bool SensorManager::isValidPressure(float pressure) {
  return (pressure >= PRESSURE_MIN && pressure <= PRESSURE_MAX) && !isnan(pressure) && !isinf(pressure);
}

bool SensorManager::isValidIAQ(float iaq) {
  return (iaq >= IAQ_MIN && iaq <= IAQ_MAX) && !isnan(iaq) && !isinf(iaq);
}

bool SensorManager::isValidCO2(float co2) {
  return (co2 >= CO2_MIN && co2 <= CO2_MAX) && !isnan(co2) && !isinf(co2);
}

bool SensorManager::isValidVOC(float voc) {
  return (voc >= VOC_MIN && voc <= VOC_MAX) && !isnan(voc) && !isinf(voc);
}

bool SensorManager::isValidPM(uint16_t pm) {
  return (pm >= PM_MIN && pm <= PM_MAX);
}

bool SensorManager::validateSensorData(const SensorData& data) {
  bool valid = true;
  
  if (data.bme68xAvailable) {
    if (!isValidTemperature(data.temperature)) {
      DEBUG_WARN("Invalid temperature: %.2f", data.temperature);
      valid = false;
    }
    if (!isValidHumidity(data.humidity)) {
      DEBUG_WARN("Invalid humidity: %.2f", data.humidity);
      valid = false;
    }
    if (!isValidPressure(data.pressure)) {
      DEBUG_WARN("Invalid pressure: %.2f", data.pressure);
      valid = false;
    }
    if (!isValidIAQ(data.iaq)) {
      DEBUG_WARN("Invalid IAQ: %.2f", data.iaq);
      valid = false;
    }
    if (!isValidCO2(data.co2Equivalent)) {
      DEBUG_WARN("Invalid CO2: %.2f", data.co2Equivalent);
      valid = false;
    }
    if (!isValidVOC(data.breathVocEquivalent)) {
      DEBUG_WARN("Invalid VOC: %.2f", data.breathVocEquivalent);
      valid = false;
    }
  }
  
  if (data.pms5003Available) {
    if (!isValidPM(data.pm1_0) || !isValidPM(data.pm2_5) || !isValidPM(data.pm10)) {
      DEBUG_WARN("Invalid PM values: PM1.0=%d, PM2.5=%d, PM10=%d", 
                 data.pm1_0, data.pm2_5, data.pm10);
      valid = false;
    }
  }
  
  if (data.ds18b20Available) {
    if (!isValidTemperature(data.externalTemp)) {
      DEBUG_WARN("Invalid external temperature: %.2f", data.externalTemp);
      valid = false;
    }
  }
  
  return valid;
}

#endif