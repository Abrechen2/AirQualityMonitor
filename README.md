# 🌪️ ESP32 Air Quality Monitor v6.0

![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-blue) ![Sensors](https://img.shields.io/badge/Sensors-3x-green) ![Status](https://img.shields.io/badge/Status-Production-brightgreen)

Ein fortschrittlicher Luftqualitätssensor basierend auf ESP32-WROOM-32 mit drei präzisen Sensoren für umfassende Umweltüberwachung.

## 📋 Überblick

Dieses Projekt implementiert eine komplette Luftqualitätsmonitoringstation mit:
- **Echte CO₂ und TVOC Werte** (berechnet durch BME680 + BSEC)
- **Feinstaub-Messung** (PM1.0, PM2.5, PM10)
- **Präzise Temperaturmessung** über externen DS18B20
- **Binäre Datenübertragung** für minimale Latenz
- **OLED Display** für lokale Anzeige
- **RGB LED Status-Anzeige**

## 🔧 Hardware-Komponenten

### Hauptplatine
- **ESP32-WROOM-32** - Mikrocontroller mit WiFi

### Sensoren
| Sensor | Typ | Messwerte |
|--------|-----|-----------|
| **BME680** | 4-in-1 Umweltsensor | Temperatur, Luftfeuchtigkeit, Luftdruck, Gas-Widerstand |
| **PMS5003** | Feinstaubsensor | PM1.0, PM2.5, PM10 µg/m³ |
| **DS18B20** | Präzisions-Temperatursensor | Externe Temperatur (±0.5°C) |

### Ausgabegeräte
- **SH1106 OLED Display** (128x64) - Lokale Datenanzeige
- **WS2812B RGB LEDs** - Status- und Qualitätsanzeige

## 🌟 Besondere Features

### ✨ BSEC-Algorithmus Integration
- **Bosch BSEC Library** für präzise Luftqualitätsmessung
- **IAQ Index** (Indoor Air Quality)
- **CO₂-Äquivalent** und **TVOC-Äquivalent** Berechnung
- **Adaptiver Kalibrierungsalgorithmus**

### 📡 Optimierte Datenübertragung
- **44-Byte Binärprotokoll** für minimalen Overhead
- **Checksumme-Validierung** für Datenintegrität
- **WiFi Auto-Reconnect** mit Fallback-Modi

### 🔋 Energieeffizienz
- **BSEC ULP Mode** (Ultra Low Power)
- **PMS5003 Sleep-Modus** zwischen Messungen
- **Adaptives Sensor-Timing**

## 📊 Gemessene Werte

### Luftqualität (BME680 + BSEC)
- **IAQ**: 0-500 (Indoor Air Quality Index)
- **CO₂-Äquivalent**: 400-40000 ppm
- **TVOC-Äquivalent**: 0-60 mg/m³
- **Genauigkeits-Indikatoren** für jeden Wert

### Umweltdaten
- **Temperatur**: -40°C bis +85°C (BME680 kompensiert)
- **Luftfeuchtigkeit**: 0-100% rH (±3%)
- **Luftdruck**: 300-1100 hPa (±1.0 hPa)
- **Externe Temperatur**: DS18B20 (±0.5°C)

### Feinstaub (PMS5003)
- **PM1.0**: Partikel ≤1.0µm
- **PM2.5**: Partikel ≤2.5µm  
- **PM10**: Partikel ≤10µm

## 🔗 Installation

### 1. Hardware-Verbindungen
```
BME680:  SDA → GPIO21, SCL → GPIO22
PMS5003: RX → GPIO16, TX → GPIO17
DS18B20: Data → GPIO4
OLED:    SDA → GPIO21, SCL → GPIO22
LEDs:    Data → GPIO5
```

### 2. Software-Requirements
- **Arduino IDE** oder **PlatformIO**
- **ESP32 Board Package**
- **Libraries**: BSEC, PMS, DallasTemperature, U8g2lib, NeoPixel

### 3. Konfiguration
1. Repository klonen:
```bash
git clone https://github.com/Abrechen2/AirQualityMonitor.git
cd AirQualityMonitor
```

2. `secrets.h` aus Template erstellen:
```bash
cp secrets_template.h secrets.h
```

3. WiFi-Credentials in `secrets.h` eintragen:
```cpp
// ===== WIFI KONFIGURATION =====
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "IHR_PASSWORT_HIER_EINTRAGEN"

// ===== NODE-RED ENDPUNKTE =====
#define NODERED_SEND_URL "http://IHR_SERVER:1880/sensor-data"
#define NODERED_AQI_URL "http://IHR_SERVER:1880/calculate-aqi"
```

4. Upload des Codes auf ESP32

### 4. Kalibrierung
- **BME680**: Automatische BSEC-Kalibrierung über 4-7 Tage
- **State-Persistierung** im EEPROM alle 6 Stunden
- **CO₂/TVOC Genauigkeit** verbessert sich mit der Zeit

## 🛠️ Debugging

- Das Verhalten der seriellen Debug-Ausgaben kann über `DEBUG_ENABLED` in `config.h` gesteuert werden.
- Zusätzliche Makros `DEBUG_INFO`, `DEBUG_WARN` und `DEBUG_ERROR` liefern klar formatierte Ausgaben zur leichteren Fehleranalyse.

## 📈 Datenformat

### Binäre Übertragung (44 Bytes)
```
Header (4B) + BME680 (24B) + DS18B20 (3B) + PMS5003 (7B) + System (5B) + Checksum (1B)
```

### JSON API für AQI-Berechnung
```json
{
  "pm2_5": 15,
  "pm10": 25,
  "iaq": 75,
  "co2": 650,
  "calibrated": true
}
```

## 🎯 Anwendungsgebiete

- **Smart Home Integration**
- **Büro-Luftqualitätsüberwachung**
- **Allergie- und Asthmaprävention**
- **HVAC-System Optimierung**
- **Luftfilter-Effizienz Monitoring**

## 📋 Status-LEDs

| Farbe | Bedeutung |
|-------|-----------|
| 🟢 Grün | Excellent (IAQ 0-50) |
| 🟡 Gelb | Good (IAQ 51-100) |
| 🟠 Orange | Lightly Polluted (IAQ 101-150) |
| 🔴 Rot | Moderately Polluted (IAQ 151-200) |
| 🟣 Lila | Heavily Polluted (IAQ 201-300) |
| ⚫ Dunkelrot | Severely Polluted (IAQ 300+) |

## 🛠️ Fehlerbehebung

### WiFi-Verbindungsprobleme
- SSID und Passwort in `secrets.h` prüfen
- Router-Kompatibilität (2.4GHz erforderlich)
- Signal-Stärke überprüfen

### Sensor-Fehler
- I2C-Verbindungen kontrollieren
- Sensor-Status im Serial Monitor prüfen
- Power-Supply (3.3V/5V) verifizieren

### BSEC-Kalibrierung
- **Erste 4 Stunden**: Genauigkeit = 0-1 (unzuverlässig)
- **Nach 24h**: Genauigkeit = 2 (verwendbar)
- **Nach 7 Tagen**: Genauigkeit = 3 (optimal)

## 📁 Projektstruktur

```
AirQualityMonitor/
├── AirQualityMonitor.ino    # Hauptprogramm
├── config.h                 # Hardware-Konfiguration
├── secrets_template.h       # Template für sensible Daten
├── SensorManager.h          # Sensor-Verwaltung
├── DisplayManager.h         # OLED-Display
├── ButtonHandler.h          # Button-Steuerung
├── LEDManager.h            # RGB-LED Steuerung
├── WiFiManager.h           # WiFi-Verbindungsmanagement
├── ByteTransmission.h      # Binäre Datenübertragung
├── DATENPUNKTE.md          # Dokumentation der Datenpunkte
├── LICENSE                 # MIT-Lizenz
└── README.md               # Diese Datei
```

## 🔄 Updates und Wartung

- **BSEC State Backup**: Automatisch alle 6h im EEPROM
- **Sensor-Kalibrierung**: Kontinuierlich während Betrieb
- **OTA Updates**: Über WiFi möglich (optional)

## 🤝 Beitragen

Contributions sind willkommen! Bitte:
1. Fork das Repository
2. Erstelle einen Feature-Branch
3. Committe deine Änderungen
4. Erstelle einen Pull Request



## 👨‍💻 Autor

**Abrechen2**  
Version 0.9 - Complete Stealth & Gas Sensor Integration + Byte Transmission

## 📄 Lizenz

Dieses Projekt ist unter der MIT-Lizenz veröffentlicht. Siehe [LICENSE](LICENSE) für Details.

## 📝 Support

Bei Fragen oder Problemen:
- Erstelle ein Issue in diesem Repository
- Überprüfe die Dokumentation in den Header-Dateien
- Konsultiere [DATENPUNKTE.md](DATENPUNKTE.md) für technische Details

---

**⚠️ Wichtiger Hinweis**: Stelle sicher, dass `secrets.h` niemals ins Repository committed wird, um deine Zugangsdaten zu schützen!

*Für detaillierte Informationen zu den Datenpunkten siehe [DATENPUNKTE.md](DATENPUNKTE.md)*