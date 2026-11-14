# Battmon — Arduino IDE sketch for ESP32-C6 Mini

Copy `Battmon/BattmonConfig.example.h` to `Battmon/BattmonConfig.h` and edit its
settings. The local `BattmonConfig.h` is ignored by Git and contains device
credentials and hardware calibration, so it will not be committed. Keep that
file beside `Battmon.ino`, then open `Battmon/Battmon.ino` in Arduino IDE. The
sketch reads three ADS1015s at
`0x48`, `0x49`, and `0x4A`. On each converter it reads the two differential
pairs (A0−A1 and A2−A3) once per second and prints each pair's raw value,
measured voltage, and calibrated value to the Serial Monitor and web page. The
first five pairs are reported in volts; ADS1015 `0x4A` A2−A3 is reported in
amperes. Pins, sample rates, AP credentials, OTA username, ADC settings, sensor
labels, and calibration factors are in `BattmonConfig.h`. For the current channel,
`kAds4AA2A3CurrentOffsetVolts` is subtracted from measured volts before
applying `kAds4AA2A3AmpsPerVolt`.

## Arduino IDE setup

1. Install the **esp32 by Espressif Systems** board package in Boards Manager.
2. In Library Manager, install **Adafruit ADS1X15** (and its Adafruit BusIO
   dependency if prompted).
3. Open `Battmon/Battmon.ino` and select the ESP32-C6 board matching your Mini.
   For a generic module, use **ESP32C6 Dev Module**.
4. Set **Partition Scheme** to **Default 4MB with spiffs** (this provides two
   OTA application slots), enable **USB CDC On Boot**, select the board's serial
   port, and use USB **Upload** for the first install.
5. Open Serial Monitor at **115200 baud**.

## Wi-Fi dashboard

After startup, connect a phone or computer to the access point:

- **Network and password:** set in `BattmonConfig.h`
- **Dashboard:** `http://192.168.4.1/`

The page shows five named battery voltage cards plus the current sensor card,
with ADS1015 address and input pair shown on each card. Values refresh every second.
It also shows accumulated charge in Ah, integrated from the calibrated current
reading using the time between samples. Positive current adds to the total and
negative current subtracts. Use **Reset Ah** to zero the counter; it starts at
zero again after a device reboot.

The dashboard's **Actual power** value is calculated as the calibrated current
in amperes multiplied by the sum of the five calibrated voltage readings. It is
shown in watts and is unavailable if any required sensor reading is missing.
The dashboard also plots total voltage and total power over the latest hour.
The chart stores one point every `kHistorySampleIntervalMs` (10 seconds by
default); history is held in RAM and starts fresh after a reboot.

## OTA firmware update

After installing a build with OTA support over USB, export a compiled binary
from Arduino IDE (**Sketch → Export Compiled Binary**). Connect to the Battmon
access point and open the dashboard, then choose **Firmware OTA** or go directly
to `http://192.168.4.1/update`. Select the ESP32-C6 `.bin` file and keep the
board powered until it finishes uploading and restarts. At the browser's
authentication prompt, use the OTA username and access point password from
`BattmonConfig.h`. OTA
requires the two application slots provided by the **Default 4MB with spiffs**
partition scheme.

## Wiring

| ADS1015 | ESP32-C6 Mini |
| --- | --- |
| SDA | GPIO 20 |
| SCL | GPIO 19 |

Set the converters' ADDR pins to GND (`0x48`), VDD (`0x49`), and SDA (`0x4A`),
respectively. For each differential measurement, connect the positive side to
A0 or A2 and the negative side to A1 or A3, respectively. The configured ADC
gain is ±2.048 V (the closest supported setting to ±2 V); keep each input pin
within the ADS1015 supply rails regardless of the differential range. A battery
still needs suitable input protection and a divider if its voltage exceeds the
ADS1015 rails.
