# ESP32 Air Quality Monitor v1.5.5

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue) ![Sensors](https://img.shields.io/badge/Sensors-3x-green) ![Status](https://img.shields.io/badge/Status-Production-brightgreen) ![Version](https://img.shields.io/badge/Version-1.5.5-blue)

Enclosure on Printables: <https://www.printables.com/model/1400485-esp32-air-quality-monitor-beluftetes-sensorgehause>

An advanced air-quality sensor based on the ESP32-WROOM-32 with three precise sensors for comprehensive environmental monitoring.

## Overview

This project implements a complete air-quality monitoring station with:
- **Real CO2 and TVOC values** (calculated with BME680/BME688 + BSEC)
- **Particulate matter measurement** (PM1.0, PM2.5, PM10)
- **Precise temperature measurement** via external DS18B20
- **MQTT Home Assistant integration** with automatic discovery (50+ entities)
- **Internal AQI calculation** (no external dependencies)
- **OLED display** for local visualization with 5 views
- **RGB LED status indicator** with smooth color transitions
- **Stealth mode** (display and LED off)

## Hardware Components

### Main Board
- **ESP32-WROOM-32** – microcontroller with Wi-Fi

### Sensors
| Sensor | Interface | Pin | Measurements |
|--------|-----------|-----|--------------|
| **BME680/BME688** | I2C | SDA=21, SCL=22 | Temperature, humidity, pressure, gas resistance, IAQ, CO2eq, TVOC |
| **PMS5003** | UART | RX=16, TX=17 | PM1.0, PM2.5, PM10 µg/m³ |
| **DS18B20** | 1-Wire | GPIO27 | External temperature (±0.5 °C) |

### Output Devices
| Component | Interface | Pin |
|-----------|-----------|-----|
| **SH1106 OLED** (128×64) | I2C | SDA=21, SCL=22 |
| **WS2812B RGB LEDs** (3x) | GPIO | GPIO5 |
| **Button** | GPIO | GPIO33 |

## Key Features

### BSEC Algorithm Integration
- **Bosch BSEC library** (LP mode, 3s interval) for precise air-quality measurement
- **IAQ index** (Indoor Air Quality, 0-500)
- **CO2 equivalent** and **TVOC equivalent** calculation
- **Adaptive calibration** — state persisted to EEPROM every 6 hours

### MQTT Home Assistant Integration
- **Automatic device discovery** via MQTT discovery protocol
- **50+ sensor entities** including calculated values, alerts, and system status
- **Real-time updates** every 10 seconds
- **Wi-Fi auto-reconnect** with fallback modes
- **All calculations performed on ESP32** — no external dependencies
- **`has_entity_name: true`** + `object_id` for clean, predictable entity IDs

### OLED Display Views
| View | Content |
|------|---------|
| OVERVIEW | AQI, level, temperature, humidity, CO2, PM2.5 |
| ENVIRONMENT | DS18B20 temp, BME68X temp/humidity/pressure |
| PARTICLES | PM1.0, PM2.5, PM10, AQI |
| GAS | IAQ, static IAQ, CO2, VOC, gas resistance, calibration |
| SYSTEM | Uptime, WiFi, IP, sensor count, firmware version |

### LED Status Colors
| Color | IAQ Range | Meaning |
|-------|-----------|---------|
| Green | 0-50 | Excellent |
| Yellow | 51-100 | Good |
| Orange | 101-150 | Lightly polluted |
| Red | 151-200 | Moderately polluted |
| Purple | 201-300 | Heavily polluted |
| Dark red | 300+ | Severely polluted |

## Installation

### 1. Hardware Connections
```
BME680/688:  SDA → GPIO21, SCL → GPIO22 (addr 0x76 or 0x77)
PMS5003:     RX → GPIO16, TX → GPIO17
DS18B20:     Data → GPIO27 (with 4.7kΩ pull-up)
OLED:        SDA → GPIO21, SCL → GPIO22 (addr 0x3C)
LEDs:        Data → GPIO5
Button:      GPIO33 (internal pull-up)
```

### 2. Required Libraries
- BSEC Software Library (Bosch Sensortec)
- PMS (PMS5003 sensor)
- DallasTemperature + OneWire (DS18B20)
- U8g2lib (OLED display)
- Adafruit NeoPixel (WS2812B LEDs)
- ArduinoJson
- PubSubClient (MQTT)
- ArduinoOTA

### 3. Configuration

Create `secrets.h` from the template:
```bash
cp secrets_template.h secrets.h
```

Fill in credentials:
```cpp
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define MQTT_BROKER_HOST "192.168.1.100"
#define MQTT_BROKER_PORT 1883
#define HOSTNAME        "ENV-Room_Name"
#define MQTT_TOPIC      "ENV-Room_Name"   // must match HOSTNAME
#define OTA_PASSWORD    "yourpassword"
```

### 4. Compile and Flash

**Compile:**
```bash
ARDUINO_CLI="C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
"$ARDUINO_CLI" compile --fqbn esp32:esp32:esp32 \
  --build-property "build.partitions=min_spiffs" \
  --build-property "upload.maximum_size=1966080" \
  ./
```

**OTA Flash:**
```bash
ESPOTA="C:/Users/.../Arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py"
python "$ESPOTA" -i <DEVICE_IP> -p 3232 --auth="<OTA_PASSWORD>" \
  -f ./build/AirQualityMonitor.ino.bin
```

### 5. Calibration
- **BME680/688**: automatic BSEC calibration, accurate after 24-48h
- **State persistence** in EEPROM every 6 hours (survives power cycles)
- **CO2/TVOC accuracy** improves over 4-7 days of continuous operation

## MQTT Data Format

### Topics
| Topic | Description |
|-------|-------------|
| `tele/<HOSTNAME>/state` | JSON payload, published every 10s |
| `tele/<HOSTNAME>/LWT` | `Online` / `Offline` (retained) |
| `homeassistant/<domain>/airqualitymonitor_<MAC>_<field>/config` | Discovery configs (retained) |

### Example State Payload
```json
{
  "temperature": 21.45,
  "humidity": 48.2,
  "pressure": 956.3,
  "iaq": 51,
  "static_iaq": 49,
  "iaq_accuracy": 3,
  "static_iaq_accuracy": 3,
  "co2": 621,
  "co2_accuracy": 3,
  "voc": 0.52,
  "voc_accuracy": 3,
  "gas_resistance": 183420,
  "bsec_calibrated": 1,
  "tvoc_ppb": 520,
  "tvoc_mgm3": 0.52,
  "pm1_0": 2,
  "pm2_5": 4,
  "pm10": 5,
  "dew_point": 10.1,
  "heat_index": 21.3,
  "absolute_humidity": 8.74,
  "comfort_index": 72,
  "external_temperature": 22.06,
  "aqi_index": 26.4,
  "aqi_category": 0,
  "pm2_5_aqi": 16,
  "pm10_aqi": 4,
  "iaq_aqi": 26,
  "sensor_bme68x_available": 1,
  "sensor_ds18b20_available": 1,
  "sensor_pms5003_available": 1,
  "sensors_available_count": 3,
  "sensor_reliable": 1,
  "bme68x_stable": 1,
  "bme68x_runin_complete": 1,
  "wifi_rssi": -62,
  "wifi_connected": 1,
  "mqtt_connected": 1,
  "stealth_mode": 0,
  "display_enabled": 1,
  "current_view": 0,
  "uptime": 14523,
  "free_heap": 148320,
  "ip_address": "192.168.200.78",
  "alert_aqi": 0,
  "alert_co2": 0,
  "alert_pm25": 0,
  "alert_tvoc": 0,
  "alert_humidity_low": 0,
  "alert_humidity_high": 0,
  "ventilation_needed": 0
}
```

### Home Assistant Entities
The device publishes discovery configs for all sensors automatically. Entity IDs follow the pattern `<domain>.env_<room>_<field>`.

**Environmental:**
`temperature`, `humidity`, `pressure`, `external_temperature`, `dew_point`, `heat_index`, `absolute_humidity`

**Air Quality (BSEC):**
`iaq`, `static_iaq`, `co2`, `voc`, `tvoc_mgm3`, `tvoc_ppb`, `gas_resistance`, accuracy indicators

**Particulate Matter:**
`pm1_0`, `pm2_5`, `pm10`, `pm2_5_aqi`, `pm10_aqi`

**AQI:**
`aqi_index`, `aqi_category`, `iaq_aqi`

**Comfort:**
`comfort_index`

**System:**
`uptime`, `free_heap`, `wifi_rssi`, `ip_address`, `current_view`, `sensors_available_count`

**Binary sensors:**
`sensor_bme68x_available`, `sensor_ds18b20_available`, `sensor_pms5003_available`, `bsec_calibrated`, `sensor_reliable`, `bme68x_stable`, `bme68x_runin_complete`, `wifi_connected`, `mqtt_connected`, `stealth_mode`, `display_enabled`

**Alerts:**
`alert_aqi`, `alert_co2`, `alert_pm25`, `alert_tvoc`, `alert_humidity_low`, `alert_humidity_high`, `ventilation_needed`

## Debugging

Serial debug output is controlled via `DEBUG_ENABLED` in `config.h`. Macros `DEBUG_INFO`, `DEBUG_WARN`, and `DEBUG_ERROR` provide formatted logs.

## Schematics & Layout

All KiCad files are in the [Schematics](Schematics) directory:
- [MainPCB-Schematic.pdf](Schematics/MainPCB-Schematic.pdf)
- [MainPCB-Layout.pdf](Schematics/MainPCB-Layout.pdf)

## Project Structure

```
AirQualityMonitor/
├── AirQualityMonitor.ino    # Main program
├── config.h                 # Hardware and timing configuration
├── secrets.h                # Credentials (not tracked in git)
├── secrets_template.h       # Template for secrets.h
├── SensorManager.h          # BME680/688, DS18B20, PMS5003 management
├── DisplayManager.h         # SH1106 OLED display and NeoPixel LEDs
├── ButtonHandler.h          # Button handling (view switch, stealth toggle)
├── LEDManager.h             # RGB LED control with smooth transitions
├── WiFiManager.h            # WiFi connection and reconnect logic
├── MQTTManager.h            # MQTT, Home Assistant discovery, data publish
├── Calculations.h           # AQI, comfort, dew point, heat index
├── TimeUtils.h              # Uptime helpers
├── DATENPUNKTE.md           # Detailed data point documentation (German)
├── CLAUDE.md                # Device inventory and OTA procedure
├── Schematics/              # KiCad project and PDFs
├── Printdata/               # STL and STEP files for enclosure
├── Pictures/                # Photos of the device
├── LICENSE                  # MIT license
└── README.md                # This file
```

## Troubleshooting

### Wi-Fi Connection Issues
- Check SSID and password in `secrets.h`
- 2.4 GHz only — 5 GHz not supported by ESP32-WROOM-32
- Verify signal strength (`wifi_rssi` entity in HA)

### Sensor Not Detected
- Check I2C connections (SDA=21, SCL=22)
- BME680/688: address must be 0x76 or 0x77 — system auto-detects both
- PMS5003: check UART wiring (RX/TX crossed), verify 5V power supply

### BSEC Calibration
| Phase | iaq_accuracy | Time |
|-------|-------------|------|
| Warm-up | 0 | First 5 min |
| Initial calibration | 1 | 5-30 min |
| Calibrating | 2 | Up to 24h |
| Fully calibrated | 3 | After 4-7 days |

State is saved to EEPROM on first accuracy=2 and every 6h thereafter. After power cycle, calibration resumes quickly from saved state.

### OTA Flash Fails ("No response from device")
- Verify OTA password matches `OTA_PASSWORD` in `secrets.h`
- OTA port 3232 must be reachable from same VLAN
- Ensure `secrets.h` has the correct `HOSTNAME` for the target device before compiling
- See `CLAUDE.md` for the full sequential flash procedure

## Version History

| Version | Changes |
|---------|---------|
| **v1.5.5** | Sensible MQTT decimal precision; `has_entity_name` + `object_id` for clean HA entity IDs; all fields always published; `aqi_color_code` removed; firmware version on OLED |
| **v1.5.4** | MAC read after WiFi init (fixes discovery MAC mismatch); unique MQTT client ID per device |
| **v1.5.3** | BSEC state persistence to EEPROM; config EEPROM storage |
| **v1.5.2** | `expire_after` in MQTT discovery |
| **v1.5.1** | Fix MAC read before WiFi init returning `00ff00000000` |
| **v1.2.0** | MQTT Home Assistant integration; internal AQI calculation; removed Node-RED dependency |
| **v1.1.0** | Switch BSEC from ULP to LP mode for reliable CO2/VOC output |
| **v1.0.0** | Initial release |

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Open a pull request

## Author

**Abrechen2**

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*For detailed data point documentation see [DATENPUNKTE.md](DATENPUNKTE.md)*
