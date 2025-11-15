# 🌪️ ESP32 Air Quality Monitor v1.1.0

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue) ![Sensors](https://img.shields.io/badge/Sensors-3x-green) ![Status](https://img.shields.io/badge/Status-Production-brightgreen) ![Version](https://img.shields.io/badge/Version-1.1.0-blue)

Enclosure on Printables: <https://www.printables.com/model/1400485-esp32-air-quality-monitor-beluftetes-sensorgehause>

An advanced air‑quality sensor based on the ESP32‑WROOM‑32 with three precise sensors for comprehensive environmental monitoring.

## 📋 Overview

This project implements a complete air‑quality monitoring station with:
- **Real CO₂ and TVOC values** (calculated with BME680 + BSEC)
- **Particulate matter measurement** (PM1.0, PM2.5, PM10)
- **Precise temperature measurement** via external DS18B20
- **Binary data transmission** for minimal latency
- **OLED display** for local visualization
- **RGB LED status indicator**

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

### 📡 Optimized Data Transmission
- **44‑byte binary protocol** for minimal overhead
- **Checksum validation** for data integrity
- **Wi‑Fi auto‑reconnect** with fallback modes

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
- **Libraries**: BSEC, PMS, DallasTemperature, U8g2lib, NeoPixel

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

3. Enter Wi‑Fi credentials in `secrets.h`:
```cpp
// ===== WIFI CONFIGURATION =====
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "ENTER_PASSWORD_HERE"

// ===== NODE‑RED ENDPOINTS =====
#define NODERED_SEND_URL "http://YOUR_SERVER:1880/sensor-data"
#define NODERED_AQI_URL "http://YOUR_SERVER:1880/calculate-aqi"
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
```
Header (4B) + BME680 (24B) + DS18B20 (3B) + PMS5003 (7B) + System (5B) + Checksum (1B)
```

### JSON API for AQI Calculation
```json
{
  "pm2_5": 15,
  "pm10": 25,
  "iaq": 75,
  "co2": 650,
  "calibrated": true
}
```

## 🎯 Use Cases

- **Smart home integration**
- **Office air‑quality monitoring**
- **Allergy and asthma prevention**
- **HVAC system optimization**
- **Air‑filter efficiency monitoring**

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
├── ByteTransmission.h       # Binary data transmission
├── TimeUtils.h              # Time and scheduling helpers
├── DATENPUNKTE.md          # Documentation of data points (German)
├── Schematics/              # KiCad project and PDFs
├── NodeRed/                 # Node‑RED flows
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
- **v1.1.0** (2025) – Fixed BSEC CO₂/VOC zero values issue by switching from ULP to LP mode
- **v1.0.0** (2025) – Complete Stealth & Gas Sensor Integration + Byte Transmission

## 📄 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## 📋 Changelog

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
