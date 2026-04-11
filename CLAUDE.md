# AirQualityMonitor - ESP32 Firmware

Firmware fuer ESP32-basierte Luftqualitaets-Monitoring-Geraete mit Home Assistant MQTT Auto-Discovery.

## Geraete im Betrieb

| Hostname | MQTT Topic | IP (DHCP) | Hardware MAC | Raum |
|----------|------------|-----------|--------------|------|
| ENV-Wohn_Dennis | ENV-Wohn_Dennis | 192.168.200.x | a842e3cf1210 | Wohnzimmer |
| ENV-Schlaf_Dennis | ENV-Schlaf_Dennis | 192.168.200.65 | a842e3cf12fc | Schlafzimmer |
| ENV-Prusa_Dennis | ENV-Prusa_Dennis | 192.168.200.152 | 08d1f9cb9734 | Prusa-Raum (3D-Drucker) |

## Fremde Geraete (NICHT anfassen!)

- **airquality_nowi_1** (MAC: `543ffc3f2303`) — IKEA Air Monitor mit IKEAAirMonitor-Firmware.
  Eigenes Projekt, eigene Discovery-Topics, eigene Entity-IDs in HA.
  KEIN Teil dieses Repos — niemals dessen MQTT-Topics oder HA-Entities veraendern!

## secrets.h – Vor dem Flashen anpassen!

`secrets.h` hat KEIN Git-Tracking (Credentials). Vor jedem OTA-Flash muss `HOSTNAME` und
`MQTT_TOPIC` auf das Zielgeraet gesetzt werden:

```cpp
#define HOSTNAME "ENV-Schlaf_Dennis"   // <-- Ziel-Board
#define MQTT_TOPIC "ENV-Schlaf_Dennis" // <-- Gleich wie HOSTNAME
#define OTA_PASSWORD "d64483wi"
```

**Nach dem Flash wieder auf ENV-Wohn_Dennis zuruecksetzen** (Standard-Entwicklungsgeraet).

## OTA Flash Prozedur

### Voraussetzungen
- Arduino IDE 2.x installiert (enthaelt arduino-cli und espota.py)
- ESP32-Board im gleichen Netzwerk erreichbar (192.168.200.x)
- SUPERVISOR-Token nicht noetig — OTA laeuft direkt auf das Board

### Schritt 1: Kompilieren

```bash
# arduino-cli Pfad (eingebettet in Arduino IDE 2.x)
ARDUINO_CLI="C:/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"

# Config fuer arduino-cli (data = Arduino15, user = D:/Projekte/Arduino)
ARDUINO_CONFIG="C:/Users/Dennis Wittke/.config/arduino-cli.yaml"

# Kompilieren (aus dem Projektverzeichnis)
cd "D:/Projekte/Arduino/AirQualityMonitor"
"$ARDUINO_CLI" compile --config-file "$ARDUINO_CONFIG" \
  --fqbn esp32:esp32:esp32 \
  --output-dir ./build \
  ./
```

Ausgabe-Binary: `./build/AirQualityMonitor.ino.bin`

### Schritt 2: OTA Upload

```bash
# espota.py Pfad (im ESP32-Core enthalten)
ESPOTA="C:/Users/Dennis Wittke/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.7/tools/espota.py"

# Upload (Ziel-IP und Auth-Passwort aus secrets.h)
python "$ESPOTA" \
  -i 192.168.200.65 \
  -p 3232 \
  --auth="d64483wi" \
  -f ./build/AirQualityMonitor.ino.bin
```

OTA-Port: **3232** (ArduinoOTA Standard).
Das Board rebooted automatisch nach erfolgreichem Upload.

### Schritt 3: Verifizieren

Nach dem Reboot pruefe in HA:
- `sensor.env_schlaf_dennis_uptime` → sollte bei ~30s sein (frischer Reboot)
- `sensor.env_schlaf_dennis_firmware_version` → sollte neue Version zeigen
- MQTT Discovery → Board publiziert neu mit `sw_version` aus `FIRMWARE_VERSION`

### Hinweise

- OTA-Port 3232 ist NUR aus dem gleichen VLAN (192.168.200.x) erreichbar,
  NICHT aus dem Management-Netz (192.168.178.x).
- Arduino IDE OTA (aus diesem PC, gleiches VLAN) funktioniert alternativ.
- arduino-cli yaml Config liegt unter:
  `C:/Users/Dennis Wittke/.config/arduino-cli.yaml`
  (data_dir: Arduino15, user_dir: D:/Projekte/Arduino)

## Arduino IDE Board-Einstellungen

| Einstellung | Wert |
|-------------|------|
| Board | ESP32 Dev Module |
| CPU Frequency | 240MHz (WiFi/BT) |
| Core Debug Level | None |
| Erase All Flash Before Upload | Disabled |
| Events Run On | Core 1 |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| JTAG Adapter | Disabled |
| Arduino Runs On | Core 1 |
| Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) |
| PSRAM | Disabled |
| Upload Speed | 115200 |
| Zigbee Mode | Disabled |

**Hinweis Partition Scheme:** Die Arduino IDE GUI nutzt "Default" (1.2MB APP-Slots).
Der arduino-cli OTA-Compile-Befehl verwendet stattdessen `min_spiffs` (1.9MB APP-Slots),
da die Firmware-Binary (~1.1MB) damit mehr Spielraum hat.
Fuer den initialen USB-Flash kann die IDE-Einstellung "Default" verwendet werden —
danach funktioniert OTA mit beiden Partition-Schemes, solange die Binary kleiner als
der OTA-Slot ist.

## Sensors & Hardware

| Sensor | Interface | Pin/Adresse | Funktion |
|--------|-----------|-------------|---------- |
| BME688 | I2C | SDA=21, SCL=22 / 0x76 oder 0x77 | Temp, Hum, Druck, Gas/IAQ (BSEC) |
| DS18B20 | 1-Wire | GPIO27 | Externe Temperatur (primaer) |
| PMS5003 | UART | RX=16, TX=17 | Partikel PM1.0/2.5/10 |
| OLED | I2C | SDA=21, SCL=22 / 0x3C | SH1106 128x64 Display |
| NeoPixel | GPIO5 | 3 LEDs | AQI-Statusanzeige |
| Button | GPIO33 | PULL_UP | View wechseln / Stealth toggle |

## MQTT-Infrastruktur

```
ESP32 Board
  └─ MQTT-Dennis (Cardinal :403)
       └─ MQTT Bridge → MQTT_Haus (Cardinal :803)
            └─ HA MQTT Integration
```

- MQTT-Dennis Broker: `192.168.178.36:403`
- Alle Discovery-Topics: `homeassistant/<domain>/airqualitymonitor_<MAC>_<field>/config`
- State-Topic: `tele/<HOSTNAME>/state` (JSON, alle Felder)
- LWT-Topic: `tele/<HOSTNAME>/LWT` (`Online`/`Offline`, retained)

## BSEC Kalibrierung

- BSEC-State wird alle 6h in EEPROM (Adresse 0) gespeichert
- Nach Neuinstallation: 24-48h Burn-in fuer valide IAQ-Werte
- `bsecCalibrated == true` wenn Accuracy >= 1

## Firmware Versionierung

`FIRMWARE_VERSION` wird **ausschliesslich** in `config.h` definiert:
```cpp
#define FIRMWARE_VERSION "1.5.6"
```
MQTTManager.h und DisplayManager.h verwenden dieses Define — kein Hardcoding!

## Versionshistorie (kurz)

| Version | Aenderung |
|---------|-----------|
| 1.5.6 | Stealth-Mode Button-Countdown (3-2-1 Overlay, triggert bei 3s Halten statt auf Release); **KRITISCHER FIX**: MQTTManager::init() liest Hostname jetzt aus EEPROM statt Konstruktor-Default — dieselbe Binary kann auf alle Boards geflasht werden |
| 1.5.5 | MQTT sinnvolle Dezimalstellen; has_entity_name+object_id → saubere entity_ids; aqi_color_code entfernt; alle Felder immer publizieren (0 wenn Sensor offline); FW-Version im OLED |
| 1.5.4 | MAC-Fix (nach WiFi-Init lesen), unique MQTT Client ID, MQTT Discovery MAC korrekt |
| 1.5.3 | BSEC State Persistenz, Config EEPROM |
| 1.5.2 | expire_after in MQTT Discovery |
| 1.5.1 | BUGFIX: MAC vor WiFi-Init gelesen (immer 00ff00000000) |
