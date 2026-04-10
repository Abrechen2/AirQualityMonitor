# 🌪️ ESP32 Air Quality Monitor v1.5.4

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue) ![Sensors](https://img.shields.io/badge/Sensors-3x-green) ![Status](https://img.shields.io/badge/Status-Production-brightgreen) ![Version](https://img.shields.io/badge/Version-1.5.4-blue)

Enclosure on Printables: <https://www.printables.com/model/1400485-esp32-air-quality-monitor-beluftetes-sensorgehause>

An advanced air‑quality sensor based on the ESP32‑WROOM‑32 with three precise sensors for comprehensive environmental monitoring.

## 📋 Overview

This project implements a complete air‑quality monitoring station with:
- **Real CO₂ and TVOC values** (calculated with BME680 + BSEC)
- **Particulate matter measurement** (PM1.0, PM2.5, PM10)
- **Precise temperature measurement** via external DS18B20
- **MQTT Home Assistant integration** with automatic discovery
- **Internal AQI calculation** (no external dependencies)
- **OLED display** for local visualization
- **RGB LED status indicator** with smooth color transitions

## 🔧 Hardware Components

### Main Board
- **ESP32‑WROOM‑32** – microcontroller with Wi‑Fi

### Sensors
| Sensor | Type | Measurements |
|--------|------|--------------|
| **BME680** | 4‑in‑1 environmental sensor | Temperature, humidity, pressure, gas resistance |
| **PMS5003** | Particulate matter sensor | PM1.0, PM2.5, PM10 µg/m³ |
| **DS18B20** | Precision temperature sensor | External temperature (±0.5 °C) |

### Output Devices
- **SH1106 OLED display** (128×64) – local data display
- **WS2812B RGB LEDs** – status and air‑quality indicator

## 🌟 Key Features

### ✨ BSEC Algorithm Integration
- **Bosch BSEC library** for precise air‑quality measurement
- **IAQ index** (Indoor Air Quality)
- **CO₂ equivalent** and **TVOC equivalent** calculation
- **Adaptive calibration algorithm**

### 📡 MQTT Home Assistant Integration
- **Automatic device discovery** via MQTT discovery protocol
- **Comprehensive sensor data** (50+ sensors including calculated values)
- **Real-time updates** every 10 seconds
- **Wi‑Fi auto‑reconnect** with fallback modes
- **All calculations performed internally** on ESP32 (no external dependencies)

### 🔋 Energy Efficiency
- **BSEC LP mode** (Low Power, 3s interval for reliable CO₂/VOC)
- **PMS5003 sleep mode** between measurements
- **Adaptive sensor timing**

## 📊 Measured Values

### Air Quality (BME680 + BSEC)
- **IAQ**: 0‑500 (Indoor Air Quality Index)
- **CO₂ equivalent**: 400‑40000 ppm
- **TVOC equivalent**: 0‑60 mg/m³
- **Accuracy indicators** for each value

### Environmental Data
- **Temperature**: ‑40 °C to +85 °C (BME680 compensated)
- **Humidity**: 0‑100 % rH (±3 %)
- **Pressure**: 300‑1100 hPa (±1.0 hPa)
- **External temperature**: DS18B20 (±0.5 °C)

### Particulate Matter (PMS5003)
- **PM1.0**: particles ≤1.0 µm
- **PM2.5**: particles ≤2.5 µm
- **PM10**: particles ≤10 µm

## 🔗 Installation

### 1. Hardware Connections
```
BME680:  SDA → GPIO21, SCL → GPIO22
PMS5003: RX → GPIO16, TX → GPIO17
DS18B20: Data → GPIO27
OLED:    SDA → GPIO21, SCL → GPIO22
LEDs:    Data → GPIO5
Button:  Select → GPIO33
```

### 2. Software Requirements
- **Arduino IDE** or **PlatformIO**
- **ESP32 board package**
  - **Libraries**: 
    - BSEC (Bosch Sensortec Environmental Cluster)
    - PMS (PMS5003 sensor library)
    - DallasTemperature (DS18B20 temperature sensor)
    - U8g2lib (OLED display)
    - NeoPixel (WS2812B RGB LEDs)
    - ArduinoJson
    - PubSubClient (MQTT)
    - ArduinoOTA (for over-the-air updates)

### 3. Configuration
1. Clone the repository:
```bash
git clone https://github.com/Abrechen2/AirQualityMonitor.git
cd AirQualityMonitor
```

2. Create `secrets.h` from the template:
```bash
cp secrets_template.h secrets.h
```

3. Enter Wi‑Fi and MQTT credentials in `secrets.h`:
```cpp
// ===== WIFI CONFIGURATION =====
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ===== MQTT CONFIGURATION =====
#define MQTT_BROKER_HOST "192.168.1.100"  // Your MQTT broker IP or hostname
#define MQTT_BROKER_PORT 1883              // MQTT port (usually 1883, use 8883 for SSL/TLS)

// Optional: MQTT authentication (comment out if not needed)
// #define MQTT_USERNAME "your_mqtt_username"
// #define MQTT_PASSWORD "your_mqtt_password"
```

4. Upload the code to the ESP32

### 4. Calibration
- **BME680**: automatic BSEC calibration over 4‑7 days
- **State persistence** in EEPROM every 6 hours
- **CO₂/TVOC accuracy** improves over time

## 🛠️ Debugging

- Serial debug output can be controlled via `DEBUG_ENABLED` in `config.h`.
- Additional macros `DEBUG_INFO`, `DEBUG_WARN` and `DEBUG_ERROR` provide clearly formatted logs for easier troubleshooting.

## 📈 Data Format

### Binary Transmission (44 bytes)
All sensor data is published as a single JSON message to the MQTT state topic:
```
homeassistant/sensor/airqualitymonitor_<MAC>/state
```

Example JSON payload:
```json
{
  "temperature": 19.8,
  "humidity": 37.3,
  "pressure": 952.83,
  "iaq": 75,
  "iaq_accuracy": 3,
  "co2": 600.92,
  "co2_accuracy": 3,
  "voc": 0.50,
  "voc_accuracy": 3,
  "pm1_0": 1,
  "pm2_5": 3,
  "pm10": 4,
  "aqi_index": 25,
  "aqi_category": 0,
  "dew_point": 4.2,
  "heat_index": 19.5,
  "absolute_humidity": 6.8,
  "comfort_index": 75,
  "wifi_connected": 1,
  "mqtt_connected": 1,
  "uptime": 3600,
  "free_heap": 150000
}
```

### Home Assistant Discovery
The device automatically publishes discovery configurations for all sensors, binary sensors, and text sensors. Home Assistant will automatically create entities for:
- **Environmental sensors**: temperature, humidity, pressure, external_temperature
- **Air quality sensors**: IAQ, CO₂, VOC, gas resistance
- **Particulate matter**: PM1.0, PM2.5, PM10
- **Calculated values**: AQI index, AQI category, dew point, heat index, absolute humidity, comfort index
- **System status**: WiFi status, MQTT status, uptime, free heap, IP address, stealth mode, display status
- **Alert sensors**: AQI alerts, CO₂ alerts, PM2.5 alerts, TVOC alerts, humidity alerts, ventilation needed

## 🎯 Use Cases

- **Smart home integration** via Home Assistant
- **Office air‑quality monitoring**
- **Allergy and asthma prevention**
- **HVAC system optimization**
- **Air‑filter efficiency monitoring**
- **Automated ventilation control** based on air quality alerts

## 📋 Status LEDs

| Color | Meaning |
|-------|---------|
| 🟢 Green | Excellent (IAQ 0‑50) |
| 🟡 Yellow | Good (IAQ 51‑100) |
| 🟠 Orange | Lightly polluted (IAQ 101‑150) |
| 🔴 Red | Moderately polluted (IAQ 151‑200) |
| 🟣 Purple | Heavily polluted (IAQ 201‑300) |
| ⚫ Dark red | Severely polluted (IAQ 300+) |

## 🛠️ Troubleshooting

### Wi‑Fi Connection Issues
- Check SSID and password in `secrets.h`
- Ensure router compatibility (2.4 GHz required)
- Verify signal strength

### Sensor Errors
- Inspect I²C connections
- Check sensor status in the serial monitor
- Verify power supply (3.3 V/5 V)

### BSEC Calibration
The BSEC algorithm requires calibration for accurate CO₂/VOC readings:

**Initial Calibration (LP Mode):**
- **First 5 minutes**: accuracy = 0 (sensor warming up)
- **5-20 minutes**: accuracy = 1 (initial calibration)
- **20+ minutes**: accuracy = 2-3 (fully calibrated)

**Important Notes:**
- Calibration state is saved to EEPROM every 6 hours
- If you change BSEC mode (ULP ↔ LP), old calibration data becomes invalid
- After mode changes, reset calibration by uncommenting `resetBsecCalibration()` in `SensorManager.h:258`
- For optimal results, let the sensor run for 24 hours in a normal environment

## 📐 Schematics & Layout

All KiCad files of the project are located in the [Schematics](Schematics) directory.
The subfolder `Schematics/KiCad` contains the complete KiCad project (`AirQualityMonitor.kicad_pro`, `.kicad_pcb`, `.kicad_sch`).
For a quick view without KiCad the following PDFs are available:

- [MainPCB-Schematic.pdf](Schematics/MainPCB-Schematic.pdf) – schematic
- [MainPCB-Layout.pdf](Schematics/MainPCB-Layout.pdf) – board layout

## 📁 Project Structure

```
AirQualityMonitor/
├── AirQualityMonitor.ino    # Main program
├── config.h                 # Hardware configuration
├── secrets_template.h       # Template for sensitive data
├── SensorManager.h          # Sensor management
├── DisplayManager.h         # OLED display
├── ButtonHandler.h          # Button control
├── LEDManager.h             # RGB LED control
├── WiFiManager.h            # WiFi connection management
├── MQTTManager.h            # MQTT and Home Assistant integration
├── Calculations.h           # Internal AQI and comfort calculations
├── TimeUtils.h              # Time and scheduling helpers
├── DATENPUNKTE.md          # Documentation of data points (German)
├── Schematics/              # KiCad project and PDFs
├── NodeRed/                 # Legacy Node‑RED flows (deprecated)
├── Printdata/               # STL and STEP files for enclosure
├── Pictures/                # Photos of the device
├── LICENSE                  # MIT license
└── README.md                # This file
```

## 🔄 Updates and Maintenance

- **BSEC state backup**: automatically every 6 h in EEPROM
- **Sensor calibration**: continuous during operation

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Open a pull request

## 👨‍💻 Author

**Abrechen2**

### Version History
- **v1.5.4** (2026) – MQTT discovery MAC mismatch fix, BSEC state improvements, PMS5003 failure tracking
- **v1.5.3** (2026) – MQTT rapid-reconnect loop fix, unique client IDs, discovery stability
- **v1.5.1** (2025) – Bugfixes and stability improvements
- **v1.2.0** (2025) – MQTT Home Assistant integration, internal AQI calculation, removed Node-RED dependency
- **v1.1.0** (2025) – Fixed BSEC CO₂/VOC zero values issue by switching from ULP to LP mode
- **v1.0.0** (2025) – Complete Stealth & Gas Sensor Integration + Byte Transmission

## 📄 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## 📋 Changelog

### v1.5.4 (2026)
**Bug Fixes:**
- Fixed critical MQTT discovery MAC mismatch: `discoveryPrefix` was computed in the constructor before the WiFi driver was initialized, causing `WiFi.macAddress()` to return garbage bytes. Discovery topic and payload carried different MACs, so Home Assistant silently discarded all discovery messages. Fixed by recomputing `discoveryPrefix` and `binarySensorDiscoveryPrefix` in `init()` after WiFi is active.
- Fixed BSEC state load on fresh flash: uninitialized EEPROM (all 0xFF) was misread as invalid length (-1 in signed format) and logged as an error. Now correctly detected as "no state saved" and handled silently.
- Fixed BSEC state not persisting across power cycles: state was only saved after the 6-hour periodic interval. Now saved immediately when IAQ accuracy first reaches ≥ 2 (typically ~30 minutes after boot).
- Fixed PMS5003 failures not surfacing to Home Assistant: consecutive read failures were logged but `pms5003Available` stayed `true`. Now set to `false` after 5 consecutive failures.

**Diagnostics:**
- Added `scanI2CBus()`: logs all I2C devices found when BME68X is not detected, to aid hardware troubleshooting.
- Added 150 ms BME68X power-up delay (shares I2C bus with display).

**MQTT Discovery Stability:**
- `reconnect()` resets `discoveryPublished` to force republish after any broker reconnect, restoring retained messages lost on broker restart.
- Mid-discovery connection check (`DISCOVERY_CHECK_CONNECTED`) aborts publish loop if broker drops, with 60 s backoff retry.
- Per-entity `mqttClient.loop()` + 10 ms delay to reduce broker overload during bulk discovery publish.

### v1.5.3 (2026)
**Bug Fixes:**
- Fixed MQTT rapid-reconnect loop caused by duplicate client IDs on multiple devices
- Fixed PMS5003 UART buffer containing stale data after firmware flash
- Unique MQTT client ID per device via hostname stored in EEPROM

### v1.5.1 (2025)
**Bugfixes:**
- Fixed MQTT discovery sensor names (removed duplicate "Air Quality Monitor" prefix)
- Fixed JSON field names to match Home Assistant discovery configuration
- Fixed missing sensor values in Home Assistant (all fields now properly published)
- Improved MQTT data publishing reliability
- Fixed const-correctness issues in MQTTManager
- Fixed display buffer send error handling

**Technical Details:**
- Simplified sensor names for better Home Assistant integration
- All JSON field names now match discovery configuration exactly
- Alert flags always included in JSON (even when 0)
- Improved error handling and code stability

### v1.2.0 (2025)
**Major Changes:**
- **Removed Node-RED dependency** - All calculations now performed internally on ESP32
- **Added MQTT Home Assistant integration** with automatic device discovery
- **Comprehensive sensor data** - 50+ sensors including calculated values, alerts, and system status
- **Internal AQI calculation** - Combined AQI from PM2.5, PM10, CO₂, VOC, and IAQ
- **Comfort calculations** - Dew point, heat index, absolute humidity, comfort index
- **Alert system** - Automatic alerts for high AQI, CO₂, PM2.5, TVOC, and humidity
- **Smooth LED transitions** - Gradual color changes for better visual feedback
- **Memory optimization** - Replaced String objects with const char* where possible

**Technical Details:**
- All AQI calculations moved to `Calculations.h` module
- MQTT discovery publishes configurations for all sensors automatically
- Single JSON state topic contains all sensor data
- Simplified field names for better Home Assistant integration
- WiFi management separated into `WiFiManager.h` module
- Removed binary transmission protocol in favor of MQTT JSON

**Breaking Changes:**
- Node-RED endpoints removed from `secrets.h`
- MQTT configuration now required
- Old Node-RED flows are deprecated (still available in `NodeRed/` folder)

### v1.1.0 (2025-11-15)
**Fixed:**
- Fixed BSEC CO₂ equivalent and VOC equivalent returning zero values
- Changed BSEC sample rate from ULP mode to LP mode (3s interval)
- Added EEPROM calibration reset function for mode changes
- Improved error handling and debug output for sensor initialization
- Fixed redundant BSEC run() calls that could cause timing issues

**Technical Details:**
- BSEC ULP mode (Ultra Low Power) does not provide reliable CO₂/VOC equivalent outputs
- Switched to BSEC LP mode (Low Power) with 3-second sampling interval
- Slightly higher power consumption (~0.3mA vs ~0.1mA) but significantly better data quality
- All BSEC outputs (IAQ, CO₂, VOC, temperature, humidity) now work correctly
- Old calibration data from ULP mode is incompatible with LP mode
- Added `resetBsecCalibration()` function to clear invalid calibration data
- Recalibration takes 5-20 minutes after reset

### v1.0.0 (2025)
- Initial release with complete sensor integration
- Stealth mode functionality
- Binary data transmission protocol
- Node-RED integration

## 📝 Support

If you have questions or problems:
- Open an issue in this repository
- Check the documentation in the header files
- Consult [DATENPUNKTE.md](DATENPUNKTE.md) for technical details

---

*For detailed information about the data points see [DATENPUNKTE.md](DATENPUNKTE.md)*
