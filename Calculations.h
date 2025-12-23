#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "SensorManager.h"

// ===== AQI CALCULATION FUNCTIONS =====

/**
 * @brief Calculate PM2.5 AQI
 * @param pm25 PM2.5 value in µg/m³
 * @return AQI value (0-500)
 */
inline uint16_t calculatePM25AQI(uint16_t pm25) {
  if (pm25 <= 12) return (uint16_t)(pm25 * 50.0f / 12.0f);
  if (pm25 <= 35) return (uint16_t)(50.0f + ((100.0f - 50.0f) / (35.4f - 12.1f)) * (pm25 - 12.1f));
  if (pm25 <= 55) return (uint16_t)(100.0f + ((150.0f - 100.0f) / (55.4f - 35.5f)) * (pm25 - 35.5f));
  if (pm25 <= 150) return (uint16_t)(150.0f + ((200.0f - 150.0f) / (150.4f - 55.5f)) * (pm25 - 55.5f));
  if (pm25 <= 250) return (uint16_t)(200.0f + ((300.0f - 200.0f) / (250.4f - 150.5f)) * (pm25 - 150.5f));
  return (uint16_t)(300.0f + ((500.0f - 300.0f) / (500.4f - 250.5f)) * (pm25 - 250.5f));
}

/**
 * @brief Calculate PM10 AQI
 * @param pm10 PM10 value in µg/m³
 * @return AQI value (0-500)
 */
inline uint16_t calculatePM10AQI(uint16_t pm10) {
  if (pm10 <= 54) return (uint16_t)(pm10 * 50.0f / 54.0f);
  if (pm10 <= 154) return (uint16_t)(50.0f + ((100.0f - 50.0f) / (154.0f - 55.0f)) * (pm10 - 55.0f));
  if (pm10 <= 254) return (uint16_t)(100.0f + ((150.0f - 100.0f) / (254.0f - 155.0f)) * (pm10 - 155.0f));
  if (pm10 <= 354) return (uint16_t)(150.0f + ((200.0f - 150.0f) / (354.0f - 255.0f)) * (pm10 - 255.0f));
  if (pm10 <= 424) return (uint16_t)(200.0f + ((300.0f - 200.0f) / (424.0f - 355.0f)) * (pm10 - 355.0f));
  return (uint16_t)(300.0f + ((500.0f - 300.0f) / (604.0f - 425.0f)) * (pm10 - 425.0f));
}

/**
 * @brief Calculate PM1.0 AQI
 * @param pm1 PM1.0 value in µg/m³
 * @return AQI value (0-500)
 */
inline uint16_t calculatePM1AQI(uint16_t pm1) {
  if (pm1 <= 8) return (uint16_t)(pm1 * 50.0f / 8.0f);
  if (pm1 <= 25) return (uint16_t)(50.0f + ((100.0f - 50.0f) / (25.0f - 8.0f)) * (pm1 - 8.0f));
  if (pm1 <= 40) return (uint16_t)(100.0f + ((150.0f - 100.0f) / (40.0f - 25.0f)) * (pm1 - 25.0f));
  if (pm1 <= 60) return (uint16_t)(150.0f + ((200.0f - 150.0f) / (60.0f - 40.0f)) * (pm1 - 40.0f));
  if (pm1 <= 100) return (uint16_t)(200.0f + ((300.0f - 200.0f) / (100.0f - 60.0f)) * (pm1 - 60.0f));
  uint16_t result = (uint16_t)(300.0f + ((500.0f - 300.0f) / (200.0f - 100.0f)) * (pm1 - 100.0f));
  return (result > 500) ? 500 : result;
}

/**
 * @brief Calculate CO2 AQI
 * @param co2 CO2 value in ppm
 * @return AQI value (0-500)
 */
inline uint16_t calculateCO2AQI(uint16_t co2) {
  if (co2 <= 400) return 25;
  if (co2 <= 600) return (uint16_t)(25.0f + ((50.0f - 25.0f) / (600.0f - 400.0f)) * (co2 - 400.0f));
  if (co2 <= 800) return (uint16_t)(50.0f + ((100.0f - 50.0f) / (800.0f - 600.0f)) * (co2 - 600.0f));
  if (co2 <= 1000) return (uint16_t)(100.0f + ((150.0f - 100.0f) / (1000.0f - 800.0f)) * (co2 - 800.0f));
  if (co2 <= 1500) return (uint16_t)(150.0f + ((200.0f - 150.0f) / (1500.0f - 1000.0f)) * (co2 - 1000.0f));
  if (co2 <= 2000) return (uint16_t)(200.0f + ((300.0f - 200.0f) / (2000.0f - 1500.0f)) * (co2 - 1500.0f));
  uint16_t result = (uint16_t)(300.0f + ((500.0f - 300.0f) / (5000.0f - 2000.0f)) * (co2 - 2000.0f));
  return (result > 500) ? 500 : result;
}

/**
 * @brief Calculate VOC AQI
 * @param voc VOC value in mg/m³
 * @return AQI value (0-500)
 */
inline uint16_t calculateVOCAQI(float voc) {
  if (voc <= 0.3f) return 25;
  if (voc <= 0.5f) return (uint16_t)(25.0f + ((50.0f - 25.0f) / (0.5f - 0.3f)) * (voc - 0.3f));
  if (voc <= 1.0f) return (uint16_t)(50.0f + ((100.0f - 50.0f) / (1.0f - 0.5f)) * (voc - 0.5f));
  if (voc <= 2.0f) return (uint16_t)(100.0f + ((150.0f - 100.0f) / (2.0f - 1.0f)) * (voc - 1.0f));
  if (voc <= 3.0f) return (uint16_t)(150.0f + ((200.0f - 150.0f) / (3.0f - 2.0f)) * (voc - 2.0f));
  if (voc <= 5.0f) return (uint16_t)(200.0f + ((300.0f - 200.0f) / (5.0f - 3.0f)) * (voc - 3.0f));
  uint16_t result = (uint16_t)(300.0f + ((500.0f - 300.0f) / (25.0f - 5.0f)) * (voc - 5.0f));
  return (result > 500) ? 500 : result;
}

/**
 * @brief Calculate IAQ to AQI conversion
 * @param iaq IAQ value (0-500)
 * @return AQI value (0-500)
 */
inline uint16_t calculateIAQtoAQI(float iaq) {
  if (iaq <= 50.0f) return (uint16_t)iaq;
  if (iaq <= 100.0f) return (uint16_t)(50.0f + ((100.0f - 50.0f) / 50.0f) * (iaq - 50.0f));
  if (iaq <= 150.0f) return (uint16_t)(100.0f + ((150.0f - 100.0f) / 50.0f) * (iaq - 100.0f));
  if (iaq <= 200.0f) return (uint16_t)(150.0f + ((200.0f - 150.0f) / 50.0f) * (iaq - 150.0f));
  if (iaq <= 300.0f) return (uint16_t)(200.0f + ((300.0f - 200.0f) / 100.0f) * (iaq - 200.0f));
  return (uint16_t)(300.0f + ((500.0f - 300.0f) / 200.0f) * (iaq - 300.0f));
}

/**
 * @brief Calculate combined AQI with weighted average
 * @param data Sensor data
 * @return Combined AQI value
 */
float calculateCombinedAQI(const SensorData& data);

// ===== COMFORT CALCULATION FUNCTIONS =====

/**
 * @brief Calculate dew point
 * @param tempC Temperature in Celsius
 * @param humidity Humidity in percent (0-100)
 * @return Dew point in Celsius
 */
inline float calculateDewPoint(float tempC, float humidity) {
  const float a = 17.27f;
  const float b = 237.7f;
  float alpha = ((a * tempC) / (b + tempC)) + logf(humidity / 100.0f);
  return (b * alpha) / (a - alpha);
}

/**
 * @brief Calculate heat index
 * @param tempC Temperature in Celsius
 * @param humidity Humidity in percent (0-100)
 * @return Heat index in Celsius
 */
inline float calculateHeatIndex(float tempC, float humidity) {
  float tempF = tempC * 9.0f / 5.0f + 32.0f;
  if (tempF < 80.0f) return tempC;
  
  float hi = -42.379f + 2.04901523f * tempF + 10.14333127f * humidity
           - 0.22475541f * tempF * humidity - 0.00683783f * tempF * tempF
           - 0.05481717f * humidity * humidity + 0.00122874f * tempF * tempF * humidity
           + 0.00085282f * tempF * humidity * humidity - 0.00000199f * tempF * tempF * humidity * humidity;
  
  return (hi - 32.0f) * 5.0f / 9.0f;
}

/**
 * @brief Calculate absolute humidity
 * @param tempC Temperature in Celsius
 * @param humidity Humidity in percent (0-100)
 * @return Absolute humidity in g/m³
 */
inline float calculateAbsoluteHumidity(float tempC, float humidity) {
  float satVaporPressure = 6.112f * expf((17.67f * tempC) / (tempC + 243.5f));
  float vaporPressure = (humidity / 100.0f) * satVaporPressure;
  return (2.1674f * vaporPressure) / (tempC + 273.15f);
}

/**
 * @brief Calculate comfort index (0-100)
 * @param temp Temperature in Celsius
 * @param humidity Humidity in percent
 * @param heatIndex Heat index in Celsius
 * @return Comfort index (0-100)
 */
uint8_t calculateComfortIndex(float temp, float humidity, float heatIndex);

// ===== ALERT CALCULATION =====

/**
 * @brief Alert flags structure
 */
struct AlertFlags {
  bool alert_aqi : 1;
  bool alert_co2 : 1;
  bool alert_pm25 : 1;
  bool alert_tvoc : 1;
  bool alert_humidity_low : 1;
  bool alert_humidity_high : 1;
  bool ventilation_needed : 1;
  bool reserved : 1;
};

/**
 * @brief Calculate alert flags
 * @param data Sensor data
 * @param aqi Combined AQI value
 * @return Alert flags
 */
AlertFlags calculateAlertFlags(const SensorData& data, float aqi);

// ===== COLOR CALCULATION =====

/**
 * @brief Calculate AQI color code
 * @param aqi AQI value
 * @return RGB color code (0xRRGGBB)
 */
uint32_t calculateAQIColor(float aqi);

/**
 * @brief Get AQI level string (memory-optimized, returns static string)
 * @param aqi AQI value
 * @return Pointer to static string
 */
const char* getAQILevelString(float aqi);

/**
 * @brief Get AQI category (1-5)
 * @param aqi AQI value
 * @return Category (1-5)
 */
inline uint8_t getAQICategory(float aqi) {
  if (aqi <= 50.0f) return 1;
  if (aqi <= 100.0f) return 2;
  if (aqi <= 150.0f) return 3;
  if (aqi <= 200.0f) return 4;
  return 5;
}

// ===== IMPLEMENTATION =====

float calculateCombinedAQI(const SensorData& data) {
  float totalWeightedAQI = 0.0f;
  float totalWeight = 0.0f;
  
  // PM1.0 AQI
  if (data.pms5003Available && data.pm1_0 > 0) {
    uint16_t pm1_aqi = calculatePM1AQI(data.pm1_0);
    totalWeightedAQI += pm1_aqi * 0.9f;
    totalWeight += 0.9f;
  }
  
  // PM2.5 AQI (highest weight)
  if (data.pms5003Available && data.pm2_5 > 0) {
    uint16_t pm25_aqi = calculatePM25AQI(data.pm2_5);
    totalWeightedAQI += pm25_aqi * 1.0f;
    totalWeight += 1.0f;
  }
  
  // PM10 AQI
  if (data.pms5003Available && data.pm10 > 0) {
    uint16_t pm10_aqi = calculatePM10AQI(data.pm10);
    totalWeightedAQI += pm10_aqi * 0.8f;
    totalWeight += 0.8f;
  }
  
  // CO2 AQI (weight based on accuracy)
  if (data.bme68xAvailable && data.co2Equivalent > 0) {
    uint16_t co2_aqi = calculateCO2AQI((uint16_t)data.co2Equivalent);
    float weight = (data.co2Accuracy >= 2) ? 0.9f : 0.6f;
    totalWeightedAQI += co2_aqi * weight;
    totalWeight += weight;
  }
  
  // VOC AQI (weight based on accuracy)
  if (data.bme68xAvailable && data.breathVocEquivalent > 0) {
    uint16_t voc_aqi = calculateVOCAQI(data.breathVocEquivalent);
    float weight = (data.breathVocAccuracy >= 2) ? 0.8f : 0.5f;
    totalWeightedAQI += voc_aqi * weight;
    totalWeight += weight;
  }
  
  // IAQ AQI (weight based on accuracy)
  if (data.bme68xAvailable && data.iaq > 0) {
    uint16_t iaq_aqi = calculateIAQtoAQI(data.iaq);
    float weight = (data.iaqAccuracy >= 2) ? 1.0f : 0.7f;
    totalWeightedAQI += iaq_aqi * weight;
    totalWeight += weight;
  }
  
  if (totalWeight > 0.0f) {
    float combined = totalWeightedAQI / totalWeight;
    return (combined < 25.0f) ? 25.0f : combined;
  }
  
  return 25.0f; // Fallback
}

uint8_t calculateComfortIndex(float temp, float humidity, float heatIndex) {
  float score = 1.0f;
  
  // Temperature assessment (ideal: 20-24°C)
  if (temp < 18.0f) {
    score *= 0.7f;
  } else if (temp > 26.0f) {
    score *= 0.8f;
  } else if (temp >= 20.0f && temp <= 24.0f) {
    score *= 1.0f;
  } else {
    score *= 0.9f;
  }
  
  // Humidity assessment (ideal: 40-60%)
  if (humidity < 30.0f) {
    score *= 0.6f;
  } else if (humidity > 70.0f) {
    score *= 0.7f;
  } else if (humidity >= 40.0f && humidity <= 60.0f) {
    score *= 1.0f;
  } else {
    score *= 0.85f;
  }
  
  // Heat index assessment
  if (heatIndex > temp + 2.0f) {
    score *= 0.8f;
  }
  
  return (uint8_t)(score * 100.0f);
}

AlertFlags calculateAlertFlags(const SensorData& data, float aqi) {
  AlertFlags flags = {0};
  
  flags.alert_aqi = (aqi > 100.0f);
  flags.alert_co2 = (data.bme68xAvailable && data.co2Equivalent > 1000.0f);
  flags.alert_pm25 = (data.pms5003Available && data.pm2_5 > 35);
  flags.alert_tvoc = (data.bme68xAvailable && data.breathVocEquivalent > 1.0f);
  flags.alert_humidity_low = (data.bme68xAvailable && data.humidity < 30.0f);
  flags.alert_humidity_high = (data.bme68xAvailable && data.humidity > 70.0f);
  
  // Ventilation needed if IAQ > 100 or CO2 > 800
  flags.ventilation_needed = (data.bme68xAvailable && 
                             (data.iaq > 100.0f || data.co2Equivalent > 800.0f));
  
  return flags;
}

uint32_t calculateAQIColor(float aqi) {
  uint8_t r, g, b;
  
  if (aqi <= 50.0f) {
    // Green to yellow-green
    float ratio = aqi / 50.0f;
    r = (uint8_t)(ratio * 128.0f);
    g = 255;
    b = 0;
  } else if (aqi <= 100.0f) {
    // Yellow-green to yellow
    float ratio = (aqi - 50.0f) / 50.0f;
    r = (uint8_t)(128.0f + ratio * 127.0f);
    g = 255;
    b = 0;
  } else if (aqi <= 150.0f) {
    // Yellow to orange
    float ratio = (aqi - 100.0f) / 50.0f;
    r = 255;
    g = (uint8_t)(255.0f - ratio * 129.0f);
    b = 0;
  } else if (aqi <= 200.0f) {
    // Orange to red
    float ratio = (aqi - 150.0f) / 50.0f;
    r = 255;
    g = (uint8_t)(126.0f - ratio * 126.0f);
    b = 0;
  } else if (aqi <= 300.0f) {
    // Red to purple
    float ratio = (aqi - 200.0f) / 100.0f;
    r = (uint8_t)(255.0f - ratio * 112.0f);
    g = 0;
    b = (uint8_t)(ratio * 151.0f);
  } else {
    // Dark red
    r = 128;
    g = 0;
    b = 0;
  }
  
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

const char* getAQILevelString(float aqi) {
  // Static strings - memory efficient
  if (aqi <= 25.0f) return "Sehr gut";
  if (aqi <= 50.0f) return "Gut";
  if (aqi <= 75.0f) return "Still good";
  if (aqi <= 100.0f) return "Moderate";
  if (aqi <= 125.0f) return "Unhealthy";
  if (aqi <= 150.0f) return "Unhealthy for sensitive groups";
  if (aqi <= 175.0f) return "Leicht ungesund";
  if (aqi <= 200.0f) return "Ungesund";
  if (aqi <= 250.0f) return "Sehr ungesund";
  if (aqi <= 300.0f) return "Extrem ungesund";
  return "Hazardous";
}

#endif

