# Controller — ESP-NOW receiver + 7-segment display (v2)

ESP32 firmware for the display side of the v2 prototype. It pairs with the
[Scale Node](../ScaleNode/) over **ESP-NOW**, receives weight packets, and shows
the value on an **8-digit MAX7219** seven-segment module.

The sketch banner calls this stage *"Phase 3 — ESP-NOW Receiver + Display"*.

## Project structure

```
Controller/
├── Controller.ino       setup/loop: start display, start ESP-NOW, show each new weight
├── Config.h             MAX7219 pins, brightness, serial baud, PMK/LMK strings
├── DataTypes.h          ScaleData packet (identical copy of ../ScaleNode/DataTypes.h)
├── Display.h/.cpp       MD_MAX72XX wrapper, custom digit font, DDD.DDDDD format
└── EspNowManager.h/.cpp Pairing (stores Scale MAC in NVS), ACK, DATA reception
```

## Wiring

| MAX7219 module | ESP32 GPIO |
|---|---|
| DIN | 23 (VSPI MOSI) |
| CLK | 18 (VSPI SCK) |
| CS | 5 |
| VCC / GND | 5 V / GND |

`Display` uses the hardware-SPI constructor of `MD_MAX72XX`
(`FC16_HW`, 1 device), so only `CS_PIN` is passed explicitly; DIN/CLK are the
ESP32's default VSPI pins.

## Behaviour

- **Display format** `DDD.DDDDD` — the weight is multiplied by 100 000 and
  printed as 8 zero-padded digits with the decimal point after the third digit
  (`005.12345`). Negative values are clamped to 0; the maximum is `999.99999`.
  The unit is whatever the Scale Node was calibrated in.
- **Pairing**
  - Unpaired: the first `PAIRING_REQ` carrying magic `SCL` is accepted, the
    sender MAC is saved to NVS (`scale_app/peer_mac`) and a `PAIRING_ACK` is
    sent back.
  - Paired: a `PAIRING_REQ` from the *same* MAC is re-ACKed (lets a Scale Node
    that lost its NVS recover); requests from other MACs are ignored.
- **Data** — `DATA` packets are accepted only from the paired MAC; the latest
  weight is shown and echoed to Serial at 115200 baud.

## Building

- Board: **ESP32 Dev Module**, ESP32 Arduino core **3.x**
  (`esp_now_recv_info_t` callback signature).
- Library: **MD_MAX72XX** (MajicDesigns) from the Library Manager.

## Notes

- `Config.h` defines `ESPNOW_PMK` / `ESPNOW_LMK`, but `EspNowManager.h` uses
  its own `PMK_KEY_STR` copy of the same string; encryption is not yet
  enabled (`encrypt = false`).
- There is no link-timeout indication: if the Scale Node stops sending, the
  last value stays on the display.
