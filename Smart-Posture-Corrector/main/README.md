# Smart Posture Corrector — Firmware

ESP32 (Arduino / PlatformIO) firmware for a two-sensor wearable posture corrector. It reads two MPU-6050 IMUs (upper back and neck) and computes the neck angle relative to the back. The firmware stores a calibrated baseline in flash, drives back and neck coin-vibration motors when posture stays bad, and exposes a BLE GATT service for the **Second Spine** Flutter app.

- PlatformIO project: [`posture controller/`](posture%20controller/)
- BLE protocol, modes, thresholds and pin reference: [`posture controller/README.md`](posture%20controller/README.md)

## Quick start

```bash
cd "posture controller"
pio run -t upload
pio device monitor -b 115200   # keys: c = calibrate, s = special mode, n = normal, 0-5 = level
```

| Item | Value |
|---|---|
| Board | `esp32dev` (ESP32-DevKitC-32E on the custom back board) |
| IMUs | MPU-6050 back @ `0x68`, neck @ `0x69`, I²C SDA 21 / SCL 22 @ 400 kHz |
| Motors | Back GPIO25, neck GPIO17 (NPN low-side drivers) |
| Button | GPIO16 (short = Normal↔Special, long ≥ 1.5 s = recalibrate) |
| BLE name | `PostureCorrector` |

## Module layout

| Path | Responsibility |
|---|---|
| `src/main.cpp`, `src/PostureApp.cpp` | Application state machine (Calibrating / Normal / Special), pin map, event wiring |
| `lib/Sensors` | Raw MPU-6050 register reads + complementary filter (α = 0.96, 100 Hz) |
| `lib/PostureLogic` | Calibration averaging, adaptive thresholds, 3 s / 15 s alert staging, EEPROM persistence |
| `lib/Hardware` | Debounced button, non-blocking vibration patterns |
| `lib/BLEDashboard` | GATT server: JSON notify, text commands, status string |
| `test/test_logic.cpp` | Unity tests for PostureLogic (need updating; see the project README) |
