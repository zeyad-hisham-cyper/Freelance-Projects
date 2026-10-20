# Smart Posture Corrector — Flutter BLE Integration Guide

ESP32-based posture corrector that monitors back and neck angles using two MPU6050 IMUs and provides haptic feedback via vibration motors. The Flutter app connects over **BLE** to display real-time data and control the device.

---

## 1. BLE Connection

| Item | Value |
|---|---|
| **Device Name** | `PostureCorrector` |
| **Service UUID** | `12345678-1234-1234-1234-123456789abc` |

### Connection Flow

```
1. Scan for BLE device named "PostureCorrector"
2. Connect to device
3. Discover service: 12345678-1234-1234-1234-123456789abc
4. Enable notifications on Posture Data characteristic (...0001)
5. Parse incoming JSON and update UI
6. Send commands by writing UTF-8 strings to Command characteristic (...0002)
7. On disconnect — the ESP32 automatically restarts advertising, so just re-scan
```

---

## 2. Characteristics

### 2.1 Posture Data — Notify (Real-time sensor stream)

| Field | Value |
|---|---|
| UUID | `12345678-1234-1234-1234-000000000001` |
| Properties | Read, Notify |
| Update rate | ~5 Hz (every 200 ms) |

**JSON payload:**

```json
{
  "back": 12.5,
  "neck": -3.2,
  "backTh": 15.0,
  "neckTh": 10.0,
  "mode": 1,
  "calibrating": false
}
```

| Key | Type | Description |
|---|---|---|
| `back` | float | Current back tilt angle (degrees). Positive = leaning forward |
| `neck` | float | Neck-relative angle (degrees). How much the neck deviates from back |
| `backTh` | float | Current back threshold. If `|back| > backTh` → bad posture |
| `neckTh` | float | Current neck threshold. If `|neck| > neckTh` → bad posture |
| `mode` | int | `0` = Calibrating, `1` = Normal, `2` = Special |
| `calibrating` | bool | `true` while calibration is actively running |

**Posture status logic:** posture is **bad** when `|back| > backTh` **OR** `|neck| > neckTh`.

### 2.2 Command — Write (Flutter → ESP32)

| Field | Value |
|---|---|
| UUID | `12345678-1234-1234-1234-000000000002` |
| Properties | Write |

Write one of these UTF-8 strings:

| Command | Description |
|---|---|
| `CALIBRATE` | Start calibration (see section 3) |
| `TOGGLE_MODE` | Toggle between Normal ↔ Special mode (see section 4) |
| `LEVEL:0` | Set sensitivity level to 0 (most lenient) |
| `LEVEL:1` | Set sensitivity level to 1 |
| `LEVEL:2` | Set sensitivity level to 2 |
| `LEVEL:3` | Set sensitivity level to 3 |
| `LEVEL:4` | Set sensitivity level to 4 |
| `LEVEL:5` | Set sensitivity level to 5 (strictest) |

### 2.3 Status — Read (Current mode string)

| Field | Value |
|---|---|
| UUID | `12345678-1234-1234-1234-000000000003` |
| Properties | Read |

Returns one of: `"CALIBRATING"`, `"NORMAL"`, `"SPECIAL"`

---

## 3. Calibration

**Purpose:** Capture the user's "good posture" baseline so the device knows when they deviate.

**How it works:**
1. Flutter sends `CALIBRATE` command (or user long-presses the physical button)
2. Mode changes to `CALIBRATING` (mode = 0)
3. User sits upright and still for **~2 minutes**
4. Device averages all sensor samples to compute baseline angles
5. Baseline + sensitivity level are **saved to EEPROM** (survives power off)
6. Mode automatically switches to `NORMAL` when calibration finishes
7. Both motors vibrate (triple buzz) to confirm calibration is done

**Flutter UI should:**
- Show a "Calibrate" button that sends the `CALIBRATE` command
- Display a calibration progress indicator when `calibrating == true`
- Show a success message when `mode` changes from `0` to `1`

---

## 4. Modes

| Mode | Value | Description |
|---|---|---|
| **Calibrating** | `0` | Device collecting baseline data. User must sit still & upright |
| **Normal** | `1` | Active posture monitoring with haptic alerts (see below) |
| **Special** | `2` | Periodic stretch reminder — vibrates every 5 minutes |

### Mode Switching
- **Flutter**: Send `TOGGLE_MODE` to switch between Normal ↔ Special
- **Physical button**: Short press toggles Normal ↔ Special
- Toggling is disabled during calibration

**Flutter UI should:**
- Show a mode toggle button (Normal / Special)
- Display the current mode from the `mode` field in the notify data
- Disable the toggle while `mode == 0` (calibrating)

---

## 5. Sensitivity / Adaptive Threshold Levels

The device has 6 sensitivity levels (0–5) that control how strict the posture threshold is.

| Level | Back Threshold | Neck Threshold | Description |
|---|---|---|---|
| 0 | baseline | baseline | Most lenient (default after calibration) |
| 1 | baseline − 5° | baseline − 5° | Slightly stricter |
| 2 | baseline − 10° | baseline − 10° | Moderate |
| 3 | baseline − 15° | baseline − 15° | Strict |
| 4 | baseline − 20° | baseline − 20° | Very strict |
| 5 | baseline − 25° | baseline − 25° | Strictest |

- **Minimum thresholds**: Back ≥ 15°, Neck ≥ 10° (never goes below these)
- Level is **saved to EEPROM** (survives power off)
- Send `LEVEL:N` command (e.g. `LEVEL:3`) to change

**Flutter UI should:**
- Show a slider or stepper for levels 0–5
- Save the user's preferred level locally in **SharedPreferences**
- On app launch, send the saved level to the device via `LEVEL:N`
- Display current thresholds from `backTh` / `neckTh` in the notify data

---

## 6. Posture Alert Behavior (Normal Mode)

When bad posture is detected in Normal mode, the ESP32 triggers vibration in two stages:

| Stage | Timing | Vibration Pattern |
|---|---|---|
| **Warning** | After **3 seconds** of bad posture | Double short buzz on the offending motor(s) |
| **Persistent** | After **15 seconds** of bad posture | Triple short buzz on the offending motor(s) |

- Only the relevant motor vibrates (back motor for bad back, neck motor for bad neck, both if both are bad)
- Alerts reset as soon as the user corrects their posture

**Flutter UI should:**
- Show real-time posture status (Good / Bad) based on `|back| > backTh || |neck| > neckTh`
- Optionally log posture events with timestamps for history tracking
- Save posture session records in **SharedPreferences** or local DB for analytics

---

## 7. Data the Flutter App Should Persist (SharedPreferences)

| Key | Type | Description |
|---|---|---|
| `sensitivity_level` | int | Last selected level (0–5). Send to device on reconnect |
| `session_history` | list | Array of session records (timestamps, duration, posture %) |
| `last_device_id` | string | BLE device ID for auto-reconnect |
| `user_preferences` | map | Any UI preferences (dark mode, alert sounds, etc.) |

On each app launch / reconnect:
1. Read `sensitivity_level` from SharedPreferences
2. Send `LEVEL:<saved_value>` to the device
3. Resume displaying real-time data

---

## 8. Hardware Pin Reference

| Component | GPIO | Notes |
|---|---|---|
| Vibration Motor 1 (back) | 25 | Digital on/off (NPN low-side driver) |
| Vibration Motor 2 (neck) | 17 | Digital on/off (driver on neck board) |
| Button | 16 | Internal pull-up. Short press = toggle mode, Long press = calibrate |
| MPU6050 #1 (back) | I2C 0x68 | SDA=21, SCL=22 |
| MPU6050 #2 (neck) | I2C 0x69 | SDA=21, SCL=22 |

---

## 9. Quick Reference — All BLE Commands

| Command String | Action |
|---|---|
| `CALIBRATE` | Start 2-min calibration |
| `TOGGLE_MODE` | Switch Normal ↔ Special |
| `LEVEL:0` … `LEVEL:5` | Set sensitivity (0 = lenient, 5 = strict) |
