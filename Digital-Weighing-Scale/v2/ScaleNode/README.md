# Scale Node — ESP-NOW transmitter (v2)

ESP32 firmware for the weighing side of the v2 prototype. It reads a load cell
through an **ADS1232** 24-bit ADC, detects when the reading is stable, applies a
two-point (zero + span) calibration stored in NVS, and transmits the weight to
the [Controller](../Controller/) over **ESP-NOW** after an automatic pairing
handshake.

The sketch banner calls this stage *"Phase 3 — ESP-NOW Transmitter"*.

## Project structure

```
ScaleNode/
├── ScaleNode.ino            Main sketch: wires the modules together, serial log, send timer
├── Config.h                 Pins, timing, stability, NVS keys, ESP-NOW constants (edit here first)
├── DataTypes.h              ScaleData packet + PacketType enum (identical copy in ../Controller)
├── ADS1232.h/.cpp           Bit-banged ADS1232 driver (pure hardware protocol)
├── ScaleReader.h/.cpp       Non-blocking wrapper: polls data-ready, keeps the last raw reading
├── StabilityDetector.h/.cpp Peak-to-peak stability check over a circular buffer
├── WeightCalculator.h/.cpp  weight = (raw − zeroOffset) / calibrationFactor
├── CalibrationManager.h/.cpp TARE / CAL buttons, status LEDs, calibration state machine, NVS
├── EspNowManager.h/.cpp     Broadcast pairing, controller MAC in NVS, unicast weight packets
└── README.md
```

Open the `ScaleNode` folder in the Arduino IDE (folder name matches the `.ino`)
and all files load as tabs.

## Wiring (defaults in `Config.h`)

| Function | Signal | ESP32 GPIO | Notes |
|---|---|---|---|
| ADS1232 | DOUT | 4 | data out / data-ready (LOW = ready) |
| ADS1232 | SCLK | 5 | clock driven by the ESP32 |
| ADS1232 | PDWN | 16 | HIGH = running |
| ADS1232 | SPEED | 17 | LOW = 10 SPS, HIGH = 80 SPS |
| ADS1232 | A0 | 18 | LOW = channel A, HIGH = channel B |
| Button | TARE | 19 | to GND, `INPUT_PULLUP` |
| Button | CAL | 21 | to GND, `INPUT_PULLUP` |
| LED | Green (ready / OK) | 22 | |
| LED | Red (error) | 23 | |

## What it does

- **Acquisition** — ADS1232 at 10 SPS on channel A, polled without blocking.
- **Stability** — the last `STABILITY_WINDOW_SIZE` (10) raw samples must span
  ≤ `STABILITY_THRESHOLD_RAW` (150) counts, i.e. ~1 s of quiet signal.
- **Tare** — TARE button stores the current raw reading as the zero offset
  (only when stable, otherwise red LED blinks 3×) and saves it to NVS.
- **Calibration** — CAL button starts a state machine:
  1. wait for a stable empty platform → capture zero;
  2. wait until the reading moves > 5000 counts (weight placed);
  3. wait for stability;
  4. type the reference weight on the **Serial monitor** (e.g. `5.0`);
  5. `factor = (raw − zero) / reference` is saved to NVS (`scale` namespace).
  The result is in whatever unit the reference was typed in.
- **LEDs** — green solid = idle/ready, green blinking = calibrating or
  success, red blinks = failure.
- **ESP-NOW** — see the protocol section below. Once paired, a `DATA` packet
  is sent every `ESPNOW_SEND_INTERVAL_MS` (200 ms).

There is no moving-average filter in this version; stability gating is
applied to the raw counts.

## ESP-NOW pairing and packet

```c
struct ScaleData {          // DataTypes.h — 8 bytes on ESP32
    uint8_t packetType;     // 0x01 PAIRING_REQ, 0x02 PAIRING_ACK, 0x03 DATA
    uint8_t magic[3];       // 'S','C','L'
    float   weight;         // calibrated weight (0 in pairing packets)
};
```

1. On boot the node loads the controller MAC from NVS (`scale_app/ctrl_mac`).
2. If none is stored it adds the broadcast peer and broadcasts `PAIRING_REQ`
   every 2 s.
3. The Controller answers with `PAIRING_ACK`; the node saves the sender MAC,
   removes the broadcast peer and adds the Controller as a unicast peer.
4. Subsequent boots skip pairing.

A PMK is set on both nodes, but **encryption is not enabled yet**
(`peer.encrypt = false`; LMK reserved for a later phase).

## Building

- Board: **ESP32 Dev Module** (any classic ESP32).
- ESP32 Arduino core **3.x** — the receive callback uses the
  `esp_now_recv_info_t` signature introduced in core 3.0.
- No external libraries (`WiFi`, `esp_now`, `Preferences` ship with the core).
- Serial monitor at 115200 baud, line ending *Newline* (needed for the
  calibration prompt).

## Known limitations / next steps

- Calibration value is entered over Serial — no on-device keypad/display.
- `clearPairing()` exists but nothing calls it (no un-pair button yet).
- No unit conversion, overload or ADC-fault reporting in the packet.
- ESP-NOW encryption (LMK) still to be enabled on both sides.
