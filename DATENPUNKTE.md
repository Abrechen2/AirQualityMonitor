# Datenpunkte Erklaerung - ESP32 Luftqualitaetssensor

Diese Dokumentation erklaert alle vom Sensor erfassten und berechneten Datenpunkte,
deren Bedeutung, Quelle und Praezision im MQTT-State-Topic.

**MQTT State-Topic:** `tele/<HOSTNAME>/state`  
**Aktualisierungsintervall:** 10 Sekunden

---

## BME680 / BME688 Sensor (via BSEC LP-Mode)

### Basis-Umweltdaten

#### `temperature` — BME68X Temperatur
- **Typ:** float, 2 Dezimalstellen (°C)
- **Bereich:** -40 bis +85 °C
- **Quelle:** BSEC-kompensiert (Selbsterwärmungskorrektur integriert) + Software-Offset (`DEFAULT_TEMP_CORRECTION`)
- **Hinweis:** Für absolute Raumtemperatur DS18B20 verwenden (`external_temperature`)

#### `humidity` — Luftfeuchtigkeit
- **Typ:** float, 1 Dezimalstelle (% rH)
- **Bereich:** 0–100 %
- **Genauigkeit:** ±3 % rH (20–80 % rH)
- **Quelle:** BME68X via BSEC, mit Software-Offset (`DEFAULT_HUMIDITY_CORRECTION`)

#### `pressure` — Luftdruck
- **Typ:** float, 1 Dezimalstelle (hPa)
- **Bereich:** 300–1100 hPa
- **Genauigkeit:** ±1.0 hPa
- **Quelle:** BME68X Rohwert (Pa → hPa)

#### `gas_resistance` — Gaswiderstand
- **Typ:** integer (Ω)
- **Bereich:** ~10.000–500.000 Ω
- **Bedeutung:** Rohdaten des MOX-Gassensors. Niedriger Wert = schlechtere Luftqualität / mehr flüchtige organische Verbindungen
- **Hinweis:** Nicht direkt interpretierbar — BSEC verarbeitet diesen Wert intern

### BSEC-Algorithmus Outputs

#### `iaq` — Indoor Air Quality
- **Typ:** integer (0–500)
- **Quelle:** Proprietärer Bosch BSEC Algorithmus (Gaswiderstand, Temperatur, Luftfeuchtigkeit)
- **Kalibrierung:** Verbessert sich über 4–7 Tage; State wird alle 6h in EEPROM gespeichert

| Wert | Bewertung |
|------|-----------|
| 0–50 | Excellent |
| 51–100 | Good |
| 101–150 | Lightly Polluted |
| 151–200 | Moderately Polluted |
| 201–300 | Heavily Polluted |
| 300+ | Severely Polluted |

#### `static_iaq` — Static IAQ
- **Typ:** integer (0–500)
- **Unterschied zu IAQ:** Weniger empfindlich gegenüber kurzfristigen Änderungen, besser für Langzeittrend

#### `iaq_accuracy` / `static_iaq_accuracy` — Kalibrierungsfortschritt
- **Typ:** integer (0–3)

| Wert | Bedeutung |
|------|-----------|
| 0 | Sensor läuft ein (erste ~5 min) |
| 1 | Unsichere Kalibrierung |
| 2 | Kalibrierung aktiv (~24h) |
| 3 | Vollständig kalibriert (4–7 Tage) |

#### `co2` — CO₂-Äquivalent
- **Typ:** integer (ppm)
- **Bereich:** 400–40.000 ppm
- **Wichtig:** Nicht direkt gemessen — BSEC-Schätzung auf Basis des Gaswiderstandsverlaufs

| Wert | Bewertung |
|------|-----------|
| 400–1000 ppm | Gut |
| 1000–2000 ppm | Akzeptabel |
| 2000–5000 ppm | Schlecht |
| >5000 ppm | Gesundheitsschädlich |

#### `co2_accuracy` — CO₂ Genauigkeitsstatus
- **Typ:** integer (0–3), wie `iaq_accuracy`

#### `voc` / `tvoc_mgm3` — TVOC-Äquivalent
- **Typ:** float, 2 Dezimalstellen (mg/m³)
- **Bereich:** 0–60 mg/m³
- **Vollname:** Total Volatile Organic Compounds
- **Quelle:** BSEC-Algorithmus (Gaswiderstandsverlauf)
- **Hinweis:** Relative Werte, keine absoluten Konzentrationen einzelner Substanzen

| Wert | Bewertung |
|------|-----------|
| 0–0.3 mg/m³ | Excellent |
| 0.3–1.0 mg/m³ | Good |
| 1.0–3.0 mg/m³ | Moderate |
| 3.0–25 mg/m³ | Poor |
| >25 mg/m³ | Unhealthy |

#### `tvoc_ppb` — TVOC in ppb
- **Typ:** integer (ppb)
- **Berechnung:** `voc_mgm3 * 1000`
- **Verwendung:** Für HA-Karten die ppb bevorzugen

#### `voc_accuracy` — VOC Genauigkeitsstatus
- **Typ:** integer (0–3)

#### `bsec_calibrated` — Kalibrierungsstatus
- **Typ:** binary (0/1)
- **Wert 1:** `iaqAccuracy >= 1`

---

## DS18B20 Temperatursensor

#### `external_temperature` — Externe Temperatur
- **Typ:** float, 2 Dezimalstellen (°C)
- **Bereich:** -55 bis +125 °C
- **Genauigkeit:** ±0.5 °C (-10 bis +85 °C)
- **Auflösung:** 12-bit (0.0625 °C)
- **Verwendung:** Primäre Raumtemperatur (kein Selbsterwärmungseffekt)
- **Leseintervall:** 10 Sekunden

---

## PMS5003 Feinstaubsensor

Messprinzip: Laser-Streulicht — Partikel werden gezählt und in Massenkonzentration umgerechnet.

#### `pm1_0` — PM1.0
- **Typ:** integer (µg/m³)
- **Definition:** Partikel mit Durchmesser ≤ 1.0 µm
- **Gesundheit:** Dringen bis in Lungenbläschen vor

#### `pm2_5` — PM2.5
- **Typ:** integer (µg/m³)
- **Definition:** Partikel ≤ 2.5 µm

| WHO-Grenzwert | Wert |
|---------------|------|
| Jahresmittel | 5 µg/m³ |
| 24h-Mittel | 15 µg/m³ |

#### `pm10` — PM10
- **Typ:** integer (µg/m³)
- **Definition:** Partikel ≤ 10 µm

| WHO-Grenzwert | Wert |
|---------------|------|
| Jahresmittel | 15 µg/m³ |
| 24h-Mittel | 45 µg/m³ |

---

## Berechnete Komfortwerte (intern auf ESP32)

#### `dew_point` — Taupunkt
- **Typ:** float, 1 Dezimalstelle (°C)
- **Berechnung:** Magnus-Formel aus Temperatur + Luftfeuchtigkeit
- **Bedeutung:** Unter diesem Wert kondensiert Wasserdampf

#### `heat_index` — Gefühlte Temperatur
- **Typ:** float, 1 Dezimalstelle (°C)
- **Berechnung:** Rothfusz-Regression (Temperatur + Luftfeuchtigkeit)
- **Bedeutung:** Relevanter über 27 °C bei hoher Luftfeuchtigkeit

#### `absolute_humidity` — Absolute Luftfeuchtigkeit
- **Typ:** float, 2 Dezimalstellen (g/m³)
- **Berechnung:** Aus Temperatur und relativer Luftfeuchtigkeit
- **Verwendung:** Lüftungssteuerung (innen vs. außen vergleichen)

#### `comfort_index` — Komfortindex
- **Typ:** integer (0–100)
- **Berechnung:** Kombiniert Temperatur, Luftfeuchtigkeit und Heat Index
- **100:** Optimaler Komfort; niedrigere Werte = zu warm/kalt/trocken/feucht

---

## AQI-Werte (intern berechnet)

#### `aqi_index` — Gesamt-AQI
- **Typ:** float, 1 Dezimalstelle
- **Berechnung:** Gewichtete Kombination aus PM2.5-AQI, PM10-AQI, IAQ-AQI
- **Grundlage:** US-EPA AQI Skala

#### `aqi_category` — AQI Kategorie
- **Typ:** integer (0–5)

| Wert | Bedeutung |
|------|-----------|
| 0 | Good (0–50) |
| 1 | Moderate (51–100) |
| 2 | Unhealthy for Sensitive Groups (101–150) |
| 3 | Unhealthy (151–200) |
| 4 | Very Unhealthy (201–300) |
| 5 | Hazardous (300+) |

#### `pm2_5_aqi` — PM2.5 AQI
- **Typ:** integer
- **Berechnung:** US-EPA Breakpoint-Tabelle für PM2.5

#### `pm10_aqi` — PM10 AQI
- **Typ:** integer
- **Berechnung:** US-EPA Breakpoint-Tabelle für PM10

#### `iaq_aqi` — IAQ → AQI Umrechnung
- **Typ:** integer
- **Berechnung:** Lineares Mapping von BSEC IAQ (0–500) auf AQI Skala

---

## Alert-Flags (Binary Sensors)

Alle Flags: integer 0 oder 1

| Feld | Auslöser |
|------|---------|
| `alert_aqi` | AQI > 100 (Unhealthy for Sensitive Groups) |
| `alert_co2` | CO₂ > 1500 ppm |
| `alert_pm25` | PM2.5 > 35 µg/m³ |
| `alert_tvoc` | TVOC > 1.0 mg/m³ |
| `alert_humidity_low` | Luftfeuchtigkeit < 30 % |
| `alert_humidity_high` | Luftfeuchtigkeit > 70 % |
| `ventilation_needed` | Kombination aus CO₂ oder TVOC-Alert |

---

## Sensor-Verfügbarkeit (Binary Sensors)

| Feld | Bedeutung |
|------|-----------|
| `sensor_bme68x_available` | BME680/688 initialisiert und liefert Daten |
| `sensor_ds18b20_available` | DS18B20 gefunden und liest Temperatur |
| `sensor_pms5003_available` | PMS5003 kommuniziert (0 nach 5 Fehler in Folge) |
| `bsec_calibrated` | iaqAccuracy >= 1 |
| `sensor_reliable` | Mindestens ein Sensor liefert valide Daten |
| `bme68x_stable` | iaqAccuracy >= 2 |
| `bme68x_runin_complete` | iaqAccuracy >= 3 (vollständig kalibriert) |
| `sensors_available_count` | Anzahl aktiver Sensoren (0–3) |

**Wichtig:** Alle Sensor-Felder werden immer publiziert, auch wenn der Sensor offline ist (Wert = 0). `sensor_<x>_available = 0` zeigt die Nichtverfügbarkeit an.

---

## System-Status

| Feld | Typ | Bedeutung |
|------|-----|-----------|
| `uptime` | integer (s) | Sekunden seit letztem Reboot |
| `free_heap` | integer (Bytes) | Freier Heap-Speicher |
| `wifi_rssi` | integer (dBm) | WLAN-Signalstärke |
| `wifi_connected` | binary | WLAN verbunden |
| `mqtt_connected` | binary | MQTT-Broker verbunden |
| `ip_address` | string | Aktuelle IP-Adresse |
| `stealth_mode` | binary | Stealth-Modus aktiv (Display + LEDs aus) |
| `display_enabled` | binary | OLED erkannt und aktiv |
| `current_view` | integer (0–4) | Aktuell angezeigte View (0=Overview … 4=System) |

---

## BSEC Konfiguration

**Modus:** LP (Low Power) — 3-Sekunden-Messintervall  
**Vorteil:** Zuverlässige CO₂/VOC-Ausgaben (ULP liefert hier Nullwerte)  
**State-Speicherung:** alle 6h in EEPROM (Adresse 0); zusätzlich sofort beim Erreichen von iaqAccuracy=2

Aktive BSEC-Outputs:
```
BSEC_OUTPUT_IAQ
BSEC_OUTPUT_STATIC_IAQ
BSEC_OUTPUT_CO2_EQUIVALENT
BSEC_OUTPUT_BREATH_VOC_EQUIVALENT
BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE
BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY
BSEC_OUTPUT_RAW_PRESSURE
BSEC_OUTPUT_RAW_GAS
BSEC_OUTPUT_STABILIZATION_STATUS
BSEC_OUTPUT_RUN_IN_STATUS
BSEC_OUTPUT_GAS_PERCENTAGE
```

---

## Wichtige Hinweise

- **CO₂ und TVOC sind Schätzwerte** — kein direkter Gassensor, nur BSEC-Algorithmus
- **PMS5003** ist unzuverlässig bei Luftfeuchtigkeit > 85 %
- **BME680 Selbsterwärmung** wird durch BSEC und Software-Offset (`DEFAULT_TEMP_CORRECTION = -3.5 °C`) kompensiert; DS18B20 ist die Referenztemperatur
- **Erste 24–48h nach Neuinstallation:** iaqAccuracy ≤ 1, CO₂/VOC-Werte noch unzuverlässig

---

*Dokumentiert für Firmware v1.5.5*
