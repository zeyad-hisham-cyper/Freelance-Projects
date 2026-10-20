# Second Spine — Flutter companion app

Mobile dashboard for the **Smart Posture Corrector** wearable. The app connects to the ESP32 device over Bluetooth Low Energy and shows live back and neck angles. From the app you can also calibrate the device, switch between Normal and Special mode, and set the sensitivity level.

> Firmware, BLE protocol and hardware are documented in the [project README](../README.md). The device-side protocol reference is in [`main/posture controller/README.md`](../main/posture%20controller/README.md).

## Screens

| Tab | File | Features |
|---|---|---|
| **Dashboard** | `lib/screens/home_screen.dart` | Connection chip · large Good/Bad status card (pulses on bad posture, shows a "straighten your back/neck" hint) · "Calibrating…" state · back and neck `AngleGauge`s vs. threshold · **Calibrate** and **Normal/Special** buttons (disabled while calibrating) · **Sensitivity slider 0–5** showing the live `±backTh / ±neckTh` |
| **Connect** | `lib/screens/connect_screen.dart` | Radar animation · 15 s BLE scan · device list with RSSI · connect/disconnect. After connecting it saves the device id and re-sends the stored `LEVEL:n`. |
| **History** | `lib/screens/history_screen.dart` | Posture-trend line chart (last 10 sessions, fl_chart) · session cards (date, duration, % good, Good ≥ 70 %) |

## Code structure

```
lib/
├── main.dart                 App entry: portrait lock, dark theme, bottom-nav shell (IndexedStack)
├── models/
│   ├── posture_data.dart     Parses the notify JSON; isPostureBad / isBackBad / isNeckBad
│   └── session_record.dart   startTime, durationSeconds, goodPosturePercent (+ JSON list helpers)
├── services/
│   ├── ble_service.dart      ChangeNotifier around flutter_blue_plus: permissions, scan, connect,
│   │                         service discovery, notify → Stream<PostureData>, command writes
│   └── storage_service.dart  SharedPreferences: sensitivity_level, last_device_id, session_history (max 50)
├── theme/                    AppColors (neon-on-navy palette, gradients), AppTheme (Outfit font)
└── widgets/
    ├── angle_gauge.dart      CustomPainter 270° arc gauge, full scale = 2 × threshold, glow when bad
    └── glass_card.dart       Blurred "glassmorphism" card
```

## BLE contract (must match firmware)

| | UUID |
|---|---|
| Service | `12345678-1234-1234-1234-123456789abc` |
| Posture data (notify) | `12345678-1234-1234-1234-000000000001` |
| Command (write) | `12345678-1234-1234-1234-000000000002` |
| Status (read) | `12345678-1234-1234-1234-000000000003` |

- Notify payload: `{"back":…,"neck":…,"backTh":…,"neckTh":…,"mode":0|1|2,"calibrating":bool}` at about 5 Hz
- Commands sent by the app: `CALIBRATE`, `TOGGLE_MODE`, `LEVEL:0`…`LEVEL:5`
- Posture is shown as bad when `|back| > backTh || |neck| > neckTh`. This is the same rule the firmware uses.

## Dependencies

| Package | Use |
|---|---|
| `flutter_blue_plus ^1.35.5` | BLE central |
| `permission_handler ^12.0.1` | Android 12+ `BLUETOOTH_SCAN`/`CONNECT` and legacy location |
| `shared_preferences ^2.3.4` | Local persistence |
| `fl_chart ^0.70.2` | History trend chart |
| `google_fonts ^6.2.1` | Outfit typeface |

Dart SDK `^3.10.4`.

## Build & run

```bash
flutter pub get
flutter run              # on a physical Android phone (BLE is not available in emulators)
flutter build apk        # release build (currently signed with the debug key)
```

1. Power on the device.
2. Open the **Connect** tab and tap **Scan for Devices**.
3. Choose **PostureCorrector**.
4. The app switches to the Dashboard and starts streaming.

## Known limitations / TODO

- Session recording is not implemented yet. `StorageService.addSession()` is never called, so the History tab shows "No Sessions Yet".
- `last_device_id` is stored but there is no auto-reconnect yet. `BleService.readStatus()` is also unused.
- iOS needs `NSBluetoothAlwaysUsageDescription` in `ios/Runner/Info.plist`.
- The package id is still `com.example.second_spine`, release builds use debug signing, and `test/widget_test.dart` is an empty placeholder.

---

**Author:** [@zeyad-hisham-cyper](https://github.com/zeyad-hisham-cyper) — part of the [Smart Posture Corrector](../README.md) project
