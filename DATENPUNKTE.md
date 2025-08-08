# 📊 Datenpunkte Erklärung - ESP32 Luftqualitätssensor

Diese Dokumentation erklärt detailliert alle vom Sensor erfassten Datenpunkte, deren Berechnung und Bedeutung.

## 🌡️ BME680 Sensor (BSEC-verarbeitet)

### Basis-Umweltdaten

#### **Temperatur** (`temperature`)
- **Typ**: `float` (°C)
- **Bereich**: -40°C bis +85°C
- **Genauigkeit**: ±1.0°C (0-65°C)
- **Quelle**: BME680 mit BSEC-Kompensation
- **Besonderheit**: Automatische Selbsterwärmung-Kompensation durch BSEC
- **Korrektur**: Zusätzliche Softwarekorrektur um `DEFAULT_TEMP_CORRECTION`

```cpp
currentData.temperature = bme68x.temperature + tempCorrection;
```

#### **Luftfeuchtigkeit** (`humidity`)
- **Typ**: `float` (% rH)
- **Bereich**: 0-100% rH
- **Genauigkeit**: ±3% rH (20-80% rH)
- **Hysterese**: ±1.5% rH
- **Quelle**: BME680 mit BSEC-Kompensation
- **Korrektur**: Zusätzliche Softwarekorrektur um `DEFAULT_HUMIDITY_CORRECTION`

```cpp
currentData.humidity = bme68x.humidity + humidityCorrection;
```

#### **Luftdruck** (`pressure`)
- **Typ**: `float` (hPa)
- **Bereich**: 300-1100 hPa
- **Genauigkeit**: ±1.0 hPa (900-1100 hPa)
- **Auflösung**: 0.18 Pa
- **Konvertierung**: Von Pa zu hPa

```cpp
currentData.pressure = bme68x.pressure / 100.0; // Pa → hPa
```

#### **Gas-Widerstand** (`gasResistance`)
- **Typ**: `float` (Ω)
- **Bereich**: 10-200,000 Ω
- **Zweck**: Rohdaten für BSEC-Algorithmus
- **Bedeutung**: Niedriger Widerstand = schlechtere Luftqualität

### BSEC-Algorithmus Outputs

#### **IAQ - Indoor Air Quality** (`iaq`)
- **Typ**: `float` (Index)
- **Bereich**: 0-500
- **Quelle**: Bosch BSEC Proprietary Algorithm
- **Berechnung**: Komplexer Algorithmus basierend auf Gas-Widerstand, Temperatur und Luftfeuchtigkeit
- **Kalibrierung**: Verbessert sich über 4-7 Tage

**IAQ Bewertungsskala:**
- 0-50: Excellent (Ausgezeichnet)
- 51-100: Good (Gut)
- 101-150: Lightly Polluted (Leicht verschmutzt)
- 151-200: Moderately Polluted (Mäßig verschmutzt)
- 201-300: Heavily Polluted (Stark verschmutzt)
- 300+: Severely Polluted (Extrem verschmutzt)

#### **Static IAQ** (`staticIaq`)
- **Typ**: `float` (Index)
- **Unterschied zu IAQ**: Weniger empfindlich gegenüber kurzfristigen Änderungen
- **Verwendung**: Langzeit-Trendanalyse
- **Stabilität**: Glättung über längeren Zeitraum

#### **CO₂-Äquivalent** (`co2Equivalent`)
- **Typ**: `float` (ppm)
- **Bereich**: 400-40,000 ppm
- **Berechnung**: BSEC-Algorithmus korreliert Gas-Widerstand mit typischen CO₂-Werten
- **Wichtig**: ⚠️ **NICHT direkt gemessen!** Algorithmus-basierte Schätzung

**CO₂-Bewertung:**
- 400-1000 ppm: Gut
- 1000-2000 ppm: Akzeptabel
- 2000-5000 ppm: Schlecht
- >5000 ppm: Gesundheitsschädlich

```cpp
currentData.co2Equivalent = bme68x.co2Equivalent;
```

#### **TVOC-Äquivalent** (`breathVocEquivalent`)
- **Typ**: `float` (mg/m³)
- **Bereich**: 0-60 mg/m³
- **Vollname**: Total Volatile Organic Compounds
- **Berechnung**: BSEC-Algorithmus basierend auf Gas-Sensor-Response
- **Quellen**: Farben, Reinigungsmittel, Möbel, Menschen

**TVOC-Bewertung:**
- 0-0.3 mg/m³: Excellent
- 0.3-1.0 mg/m³: Good
- 1.0-3.0 mg/m³: Moderate
- 3.0-25 mg/m³: Poor
- >25 mg/m³: Unhealthy

### Genauigkeits-Indikatoren

#### **IAQ Accuracy** (`iaqAccuracy`)
- **Typ**: `uint8_t` (0-3)
- **Bedeutung**: 
  - 0: Sensor läuft ein (erste 4h)
  - 1: Unsichere Kalibrierung
  - 2: Kalibrierung in Arbeit (nach ~24h)
  - 3: Kalibriert (nach 4-7 Tagen)

```cpp
// BSEC Kalibrierungsstatus
currentData.bsecCalibrated = (currentData.iaqAccuracy >= 2);
```

#### Weitere Genauigkeits-Flags
- `staticIaqAccuracy`: Genauigkeit des Static IAQ
- `co2Accuracy`: Genauigkeit der CO₂-Schätzung  
- `breathVocAccuracy`: Genauigkeit der TVOC-Schätzung

## 🌡️ DS18B20 Temperatursensor

#### **Externe Temperatur** (`externalTemp`)
- **Typ**: `float` (°C)
- **Bereich**: -55°C bis +125°C
- **Genauigkeit**: ±0.5°C (-10°C bis +85°C)
- **Auflösung**: 12-bit (0.0625°C)
- **Zweck**: Referenz-Temperatur ohne Selbsterwärmung
- **Vorteil**: Präziser als BME680 für absolute Temperatur

```cpp
ds18b20.requestTemperatures();
delay(750); // 12-bit conversion time
float temp = ds18b20.getTempCByIndex(0);
```

## 💨 PMS5003 Feinstaubsensor

### Funktionsprinzip: Laser-Streuung
Der PMS5003 verwendet ein Laser-Streulicht-Verfahren:
1. **Laser-Diode** beleuchtet Luftstrom
2. **Photodiode** detektiert gestreutes Licht
3. **Mikrocontroller** zählt und kategorisiert Partikel
4. **Umrechnung** in Massenkonzentration (µg/m³)

#### **PM1.0** (`pm1_0`)
- **Typ**: `uint16_t` (µg/m³)
- **Definition**: Massenkonzentration von Partikeln ≤1.0µm Durchmesser
- **Gesundheit**: Dringen tief in Lungenbläschen ein
- **Quellen**: Verbrennungsprozesse, Autoabgase

#### **PM2.5** (`pm2_5`) 
- **Typ**: `uint16_t` (µg/m³)
- **Definition**: Massenkonzentration von Partikeln ≤2.5µm Durchmesser
- **Gesundheit**: WHO-überwachter Parameter, krebserregend
- **Grenzwerte (WHO 2021)**:
  - Jahresmittel: 5 µg/m³
  - 24h-Mittel: 15 µg/m³

#### **PM10** (`pm10`)
- **Typ**: `uint16_t` (µg/m³)  
- **Definition**: Massenkonzentration von Partikeln ≤10µm Durchmesser
- **Gesundheit**: Atemwegsirritation, Asthma-Trigger
- **Grenzwerte (WHO 2021)**:
  - Jahresmittel: 15 µg/m³
  - 24h-Mittel: 45 µg/m³

**PM-Bewertungsskala:**
- 0-12 µg/m³: Gut
- 12-35 µg/m³: Mäßig
- 35-55 µg/m³: Ungesund für sensitive Gruppen
- 55-150 µg/m³: Ungesund
- 150-250 µg/m³: Sehr ungesund
- >250 µg/m³: Gefährlich

```cpp
// PMS5003 Messzyklus
pms5003.wakeUp();
delay(3000); // Stabilisierung
pms5003.requestRead();
if (pms5003.readUntil(pmsData)) {
    currentData.pm1_0 = pmsData.PM_AE_UG_1_0;  // Atmospheric Environment
    currentData.pm2_5 = pmsData.PM_AE_UG_2_5;
    currentData.pm10 = pmsData.PM_AE_UG_10_0;
}
pms5003.sleep(); // Energiesparen
```

## 🔌 System-Status Datenpunkte

#### **Verfügbarkeits-Flags**
```cpp
bool bme68xAvailable;    // BME680 funktionsfähig
bool ds18b20Available;   // DS18B20 gefunden
bool pms5003Available;   // PMS5003 kommuniziert
bool bsecCalibrated;     // BSEC Genauigkeit ≥2
```

#### **WiFi & System**
```cpp
uint32_t uptime_seconds; // Betriebszeit in Sekunden
int8_t wifi_rssi;        // WiFi Signalstärke (dBm)
```

## 📡 Datenübertragungsprotokoll

### Binäres Format (44 Bytes total)

#### **Header (4 Bytes)**
```cpp
uint16_t packet_id = 0xAA55;    // Sync-Pattern
uint8_t version = 1;            // Protokoll-Version  
uint8_t packet_type = 0x01;     // Sensor-Daten
```

#### **BME680 Block (24 Bytes)**
```cpp
int16_t temperature_x100;       // °C * 100
uint16_t humidity_x100;         // % * 100  
uint16_t pressure_x10;          // hPa * 10
uint32_t gas_resistance;        // Ω
int16_t iaq_x100;              // IAQ * 100
int16_t static_iaq_x100;       // Static IAQ * 100
uint16_t co2_equivalent;        // ppm
uint16_t breath_voc_x100;       // mg/m³ * 100
uint8_t bme_flags;             // Verfügbarkeit + Kalibrierung
```

#### **Komprimierungs-Algorithmus**
```cpp
// Temperatur: -40°C bis +85°C → int16 (-4000 bis +8500)
packet.temperature = (int16_t)(data.temperature * 100);

// Luftfeuchtigkeit: 0-100% → uint16 (0 bis 10000)  
packet.humidity = (uint16_t)(data.humidity * 100);

// Druck: 300-1100 hPa → uint16 (3000 bis 11000)
packet.pressure = (uint16_t)(data.pressure * 10);
```

### Checksumme-Validierung
```cpp
uint8_t calculateChecksum(const SensorDataPacket& packet) {
    uint8_t checksum = 0;
    const uint8_t* bytes = (const uint8_t*)&packet;
    
    // XOR aller Bytes außer Checksumme
    for (size_t i = 0; i < sizeof(SensorDataPacket) - 1; i++) {
        checksum ^= bytes[i];
    }
    return checksum;
}
```

## 🎯 AQI-Berechnung (Extern)

Der Sensor sendet Rohdaten an Node-RED für erweiterte AQI-Berechnung:

```json
{
    "pm2_5": 15,
    "pm10": 25, 
    "iaq": 75,
    "co2": 650,
    "calibrated": true
}
```

**Antwort:**
```json
{
    "aqi": 65,
    "level": "Gut",
    "color": "#00FF00",
    "dominant": "PM2.5"
}
```

## ⚠️ Wichtige Hinweise

### BSEC-Kalibrierung
- **Erste Messungen unzuverlässig** - mindestens 24h laufen lassen
- **State wird alle 6h gespeichert** für schnellere Rekalibrierung
- **Optimale Genauigkeit nach 4-7 Tagen** kontinuierlichem Betrieb

### Sensor-Limitationen
- **CO₂**: Nicht direkt gemessen, nur BSEC-Schätzung
- **TVOC**: Relativ-Werte, nicht absolute Konzentration
- **PMS5003**: Empfindlich gegen Luftfeuchtigkeit >85%
- **BME680**: Selbsterwärmung bei häufiger Messung

### Wartung
- **PMS5003**: Sleep-Modus verlängert Lebensdauer
- **BSEC State**: Backup verhindert Kalibrierungsverlust
- **Temperatur-Korrektur**: Je nach Gehäuse anpassen

---

*Diese Dokumentation beschreibt die Implementierung in Version 0.8 des ESP32 Air Quality Monitors*
