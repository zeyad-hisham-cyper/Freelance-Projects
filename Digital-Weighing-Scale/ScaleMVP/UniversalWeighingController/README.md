# Universal Weighing Controller

ESP32 firmware for a modular, configurable weighing platform.
**One firmware, many weighing systems.**

The measuring core is **load-cell reading → display → 4x4 keypad**, with
calibration persisted to flash. On top of that,
compile-time switches select what the board actually is:

The wireless link is **on by default** (`ENABLE_NRF`): the controller and the
dashboard are a pair. A board with no radio fitted still weighs — it logs the
fault and shows `RADIO` — so the switch only ever needs turning off to save
Flash.

| Build | `hardware.h` | What it is |
|---|---|---|
| Bench scale *(default)* | `DISPLAY_OLED` | SH1106 OLED indicator |
| Industrial indicator | `DISPLAY_7SEG` | MAX7219 six-digit LED panel |

The remote panel is a **separate firmware** in `../WirelessDashboard`. This
project only ever transmits; nothing here decodes a packet.

---

## Hardware / Pin Map

Taken from the validated bring-up sketches (`hardware_test/`, `oled_test/`).
All GPIO assignments live in `src/config/pins.h` — nothing else references a
pin number.

Both ADS1232 and HX711 are supported; select one in `hardware.h`.

| Function | Signal | GPIO |
|---|---|---|
| ADS1232 | DOUT  | 34 (input-only pin, correct for DOUT) |
| ADS1232 | SCLK  | 25 |
| ADS1232 | PDWN  | 26 |
| ADS1232 | SPEED | 27 |
| ADS1232 | A0    | 14 |
| HX711 | DT  | 34 |
| HX711 | SCK | 25 |
| OLED SH1106 (I²C, addr 0x3C) | SDA | 21 |
| OLED SH1106 | SCL | 22 |
| **SPI bus** (shared) | SCK  | 18 |
| **SPI bus** (shared) | MOSI | 23 |
| **SPI bus** (shared) | MISO | 19 |
| MAX7219 | DIN → MOSI | 23 |
| MAX7219 | CLK → SCK  | 18 |
| MAX7219 | CS   | 5  |
| nRF24L01+ | CE  | 16 |
| nRF24L01+ | CSN | 17 |
| nRF24L01+ | IRQ | not connected (the driver polls) |
| Keypad | Rows R0-R3 | 32, 33, 26, 27 |
| Keypad | Cols C0-C3 | 12, 13, 14, 15 |

The dashboard wires its radio to different GPIOs (4 and 15) — separate board,
separate layout. Only the SPI bus lines and the radio *settings* in
`hardware.h` have to match between the two.

### 4x4 keypad

Rows are driven LOW one at a time; columns are read with the ESP32's
internal pull-ups, so a pressed key reads LOW.

**That direction is not a free choice.** GPIO12 (MTDI) is a strapping pin: if
it is HIGH at reset the ESP32 selects a 1.8 V flash supply and the board does
not boot. So:

1. **Never fit external pull-ups to the column lines.** The internal ones are
   enabled long after reset and are harmless; an external resistor is present
   *during* reset.
2. Columns must be the inputs. With rows high-impedance until the first scan,
   a key held down at power-up cannot drag GPIO12 high — MTDI's internal
   pull-down wins.

GPIO15 (MTDO) is also a strapping pin, but only cosmetically: held low at
reset it silences the boot ROM log.

**The keypad and the ADS1232 cannot both be fitted on this pin map.** Rows
R2/R3 are the ADS1232's PDWN/SPEED and column C2 is its A0. `config.h` stops
the build with an explanatory `#error` if `USE_ADS1232` is selected. The
keypad freed only GPIO4 (the old CAL button), one pin short of what the
ADS1232 needs — so either fit the HX711, which needs two pins, or re-assign
the ADS1232 pins to whatever your board has spare.

### Sharing one SPI bus

The MAX7219 and the nRF24L01+ sit on the same three lines and are told apart
by their chip selects alone. This works because:

- every access is inside an `SPI.beginTransaction()` block with that device's
  own `SPISettings`, so neither steals the other's clock or mode;
- chip selects idle HIGH and go low only inside a transaction — the MAX7219
  latches on the **rising** edge of CS, so it ignores the radio's traffic;
- the MAX7219 has **no data output**, so it can never fight the radio for
  MISO.

`src/drivers/SpiBus.*` owns the one-time `SPI.begin()`; both drivers call it.
It also drives **both** chip selects inactive before the bus starts — the
drivers are initialised one after another, and neither CS line has a
pull-up, so without that the idle device can latch its neighbour's traffic
while it is still an undriven input.

### Two power warnings worth reading before debugging firmware

- **MAX7219 at 5 V.** Its `V_IH` is 3.5 V on a 5 V supply, and the ESP32
  drives 3.3 V — technically out of spec, though it works on most modules.
  Flickering or garbled digits means a level shifter (74AHCT125) on
  DIN/CLK/CS, or running the module at 3.3 V and accepting dimmer digits.
- **nRF24 supply.** The module draws current in bursts most ESP32 boards'
  3.3 V regulators cannot deliver cleanly. If `begin()` succeeds but every
  send fails with `no ack (max retries)`, solder a 10 µF capacitor directly
  across the module's VCC/GND pins. **Never feed its VCC from 5 V.**

---

## Building

Arduino IDE → open `UniversalWeighingController/UniversalWeighingController.ino`
→ board **ESP32 Dev Module**.

Required libraries (Library Manager):

- Adafruit GFX Library
- Adafruit SH110X

Verified against **esp32 core 3.2.1** in the default configuration:
353 050 B flash (26 %), 22 568 B RAM (6 %) — measured before the MAX7219 and
nRF24 drivers were added. Neither is compiled unless its switch is on, so the
default build should be within a few hundred bytes of that; the figure has
not been re-measured.

---

## Project Layout

The structure follows `system.md`, with `config/ drivers/ modules/ app/`
placed inside `src/` — this is the one adaptation required, because the
Arduino IDE only compiles sub-folders that live under `src/`.

```
UniversalWeighingController/
├── UniversalWeighingController.ino   entry point (delegates to App)
└── src/
    ├── config/
    │   ├── config.h        includes everything below, sanity checks
    │   ├── features.h      ENABLE_FILTER / ENABLE_EEPROM / DEBUG_SERIAL ...
    │   ├── hardware.h      cell count, ADC, display, capacity, tuning
    │   ├── pins.h          every GPIO
    │   └── calibration.h   defaults + flash record layout
    ├── protocol/
    │   └── WeightPacket.h  the wire contract (shared with the dashboard)
    ├── drivers/
    │   ├── ADS1232.*       bit-banged 24-bit ADC protocol
    │   ├── HX711.*         bit-banged 24-bit ADC protocol (2-wire)
    │   ├── MatrixKeypad.*  4x4 scanning, debounce, key-press queue
    │   ├── SpiBus.*        one-time bring-up of the shared SPI bus
    │   ├── OledDisplay.*   SH1106 panel + screen layouts
    │   ├── MAX7219.*       seven-segment panel: registers, font, layouts
    │   ├── NRF24.*         register-level nRF24L01+ transceiver
    │   └── EepromStore.*   byte-level persistence + CRC-32
    ├── modules/
    │   ├── WeightManager.*      ADCs -> filtered, averaged raw counts
    │   ├── FilterManager.*      moving average + stability detection
    │   ├── CalibrationManager.* zero, scale factor, tare, cal procedure
    │   ├── UnitManager.*        g / kg / lb / oz conversion
    │   ├── DisplayManager.*     unified display interface
    │   ├── WirelessManager.*    packet assembly, transmission, link state
    │   ├── StorageManager.*     versioned, CRC-protected config record
    │   ├── InputManager.*       the key map: which keycap means what
    │   └── DiagnosticManager.*  ADC fault, overload, radio faults
    └── app/
        ├── App.h
        ├── setup.cpp       power-on sequence
        └── loop.cpp        main cycle + UI logic
```

### Layering rule

`app → modules → drivers → hardware`, never upward and never sideways.
`WeightManager` stops at **raw counts**; `CalibrationManager` turns counts
into **grams**; `UnitManager` turns grams into the **displayed** number.
That is what makes swapping a load cell a recalibration rather than a code
change.

---

## Configuration

Everything customisable is in `src/config/`. The application logic never
needs editing.

```cpp
// hardware.h
#define USE_HX711              // or USE_ADS1232
#define DISPLAY_OLED           // or DISPLAY_7SEG
#define LOAD_CELL_COUNT   1    // 1, 2 or 4 — add pin sets in pins.h
#define SCALE_CAPACITY_G  20000.0f
#define FILTER_WINDOW_SIZE 24
#define STABILITY_THRESHOLD_COUNTS 400L
#define HX711_GAIN_SETTING 1   // channel A, gain 128 (the load-cell input)
```

### ADS1232 vs HX711

Both drivers expose the same `setPins / begin / isReady / readRaw` surface,
so `WeightManager` is identical either way. The differences that matter:

| | ADS1232 | HX711 |
|---|---|---|
| Wires | DOUT, SCLK, PDWN, SPEED, A0 | DT, SCK only |
| Sample rate | software, `ADC_SPEED_80SPS` | **hardware RATE pin on the module** — `ADC_SPEED_80SPS` is ignored |
| Channel/gain | A0 pin | number of trailing clock pulses (`HX711_GAIN_SETTING`) |
| Power down | PDWN pin | hold SCK high >60 µs |

Two HX711 consequences are handled inside the driver: the gain setting only
takes effect on the *following* conversion (so `begin()` performs and discards
one read), and the read is wrapped in a critical section because a FreeRTOS
preemption holding SCK high past 60 µs would power the chip down mid-read and
reset it to channel A / gain 128.

```cpp
// features.h
#define ENABLE_FILTER
#define ENABLE_EEPROM
#define ENABLE_DIAGNOSTICS
#define ENABLE_NRF             // on by default — the link is part of the product
#define DEBUG_SERIAL           // 115200 baud
```

`config.h` refuses to build a configuration that cannot work: two displays,
two wireless roles, a role with no radio, a channel above 125, a digit count
above 8. Every one of those is a `#error` with the fix in the message.

Adding load cells: raise `LOAD_CELL_COUNT` and add one entry to **every**
pin array in `pins.h`. Each cell gets its own ADC, its own filter, and the
channels are averaged — so the calibration factor does not depend on how
many cells are fitted.

---

## Operation

### Weighing screen

```
GROSS                    ■     <- NET when tared; ■ solid = stable
─────────────────────────────
                  12.345
                           kg
```

### Keypad

```
1 2 3 A        A = TARE    zero the platform
4 5 6 B        B = UNIT    cycle g -> kg -> lb -> oz
7 8 9 C        C = ENTER   start calibration, confirm each step
* 0 # D        D = CANCEL  clear the tare / abort calibration
```

`0`–`9` type the reference mass during calibration, `*` is the decimal point
and `#` is backspace (hold it to clear the field).

| Key | Weighing screen |
|---|---|
| **A** | store current gross as the tare offset, display goes to zero |
| **D** | clear the tare, back to gross |
| **B** | cycle g → kg → lb → oz (display only; the measurement stays in grams) |
| **C** | start the calibration procedure |

The selected unit is saved to flash and restored at the next power-up.

**There are no long presses any more.** With three buttons, "clear the tare"
and "abort calibration" had to be held, because there were not enough buttons
to give them their own. Both are now the CANCEL key, which removes the whole
short-versus-long timing question.

### Calibration procedure

Press **C** from the weighing screen. Press **D** at any point to abort.

1. **ZERO** — remove all weight, press **C**. 16 samples are averaged into
   the zero offset.
2. **LOAD** — place the reference mass, press **C**.
3. **VALUE** — **type** the mass in grams. `*` gives a decimal point, `#`
   deletes. While the field is empty the screen offers the mass used by the
   stored calibration, since most recalibrations reuse the same weight.
   Press **C** to save.

The field starts empty rather than pre-filled. Pre-filling looks helpful and
is not: the operator types their digits onto the end of a number they did not
notice was there and gets a calibration off by a factor of ten.

A value that is empty or outside `CAL_WEIGHT_MIN_G`…`SCALE_CAPACITY_G` is
rejected at the **C** press with `CAL FAIL — Bad reference mass`, before it
can reach the arithmetic.

The scale factor is `(loaded counts − zero counts) / reference grams`, stored
with magic + version + CRC-32. If the span is under `CAL_MIN_SPAN_COUNTS` the
calibration is rejected (`CAL FAIL — No weight detected`) rather than saving a
wildly wrong factor. A new calibration clears the tare.

An uncalibrated board uses `DEFAULT_COUNTS_PER_GRAM = 1.0`, so it displays
essentially raw counts — an obviously wrong reading is safer than a plausibly
wrong one.

### Diagnostics

| Screen (OLED / 7-seg) | Meaning |
|---|---|
| `WARM UP` / `WArM U` | filter window still filling (~1 s at 10 SPS) |
| `ADC FAIL` / `Err Ad` | no conversion for 3× the ADC timeout — check load-cell wiring |
| `OVERLOAD` / `Err Ov` | beyond `SCALE_CAPACITY_G × OVERLOAD_MARGIN` |
| `RADIO` / `Err rA` | no nRF24 answering on the SPI bus — wiring or supply |

A scale that cannot reach its dashboard is **not** a fault: it is still
weighing correctly, so the dropped link is reported in the serial log
(`[no link]`) and nowhere else. The dashboard firmware makes the opposite
call about the same dropped link, because a stale number on a remote display
is worse than no number.

With `DEBUG_SERIAL` on, the controller prints raw counts, net weight,
stability, timeout count and link state twice a second at 115200 baud.

---

## Seven-Segment Display (MAX7219)

Enable with `DISPLAY_7SEG`. The driver runs the chip in **no-decode** mode
and carries its own segment font, because the MAX7219's built-in BCD decoder
can only draw `0-9 - E H L P` and the indicator has to write `CAL`, `tArE`
and `Err` too.

What six digits can and cannot show:

| | |
|---|---|
| Weight | full width, right-aligned; decimals are shed automatically until the number fits (`12345.67` → `12345.7`) |
| Out of range | `OL`, centred |
| NET | the **leftmost decimal point** — the only element on the panel that is not part of the number |
| Unit, stability | not shown; a bare panel has no annunciator LEDs. Both stay in the `showWeight()` signature so the OLED and the MAX7219 are called identically |
| Calibration | `C1ZERO`, `C2LOAD`, then the reference mass across the whole panel |
| Faults | `Err` plus a two-letter tag (see the diagnostics table) |

```cpp
// hardware.h
#define MAX7219_DIGITS 6
#define MAX7219_BRIGHTNESS 8
#define MAX7219_DIGIT0_IS_RIGHTMOST 0   // 1 if text comes out mirrored
#define MAX7219_SPI_HZ 1000000          // low on purpose, see below
#define MAX7219_CONFIG_REFRESH_MS 1000
```

**Why the clock is 1 MHz and not the datasheet's 10 MHz:** the real limit is
the 3.3 V → 5 V level mismatch, not the chip. On the dashboard hardware, 8 MHz
over jumper leads corrupted the occasional word, and a corrupted word that
lands on the display-test register lights the whole panel until power is
cycled. The driver now also re-sends every control register once a second
(`MAX7219_CONFIG_REFRESH_MS`) so such a corruption self-heals. Both are a net
rather than a cure — the cure is a 74AHCT125 level shifter on DIN/CLK/CS.
See `WirelessDashboard/README.md` → *Troubleshooting the Panel*.

Positions in the driver API are counted from the **left of the panel as the
operator sees it**; the define above maps them onto the chip's DIG0..DIG7
outputs. `MAX7219::runSelfTest()` (also `DisplayManager::runSelfTest()`) runs
a lamp test, a walking segment and a 0-9 count — enough to prove wiring,
digit order and every individual segment line in one call.

---

## Wireless Link (nRF24L01+)

`ENABLE_NRF` is on by default. It adds the **transmitter** to the scale; the
receiving panel is a separate firmware in `../WirelessDashboard`. The driver
is register-level — no RF24 library — for the same reason the ADC drivers
are: every timing rule stays visible in the repository.

```cpp
// hardware.h
#define NRF_CHANNEL 76         // 2476 MHz; 0..83 is the legal ISM band
#define NRF_DATA_RATE 0        // 1 Mbps — works on clone modules too
#define NRF_PA_LEVEL 3         // 0 = -18 dBm ... 3 = 0 dBm
#define NRF_ADDRESS_SCALE     { 'S', 'C', 'A', 'L', '1' }
#define NRF_ADDRESS_DASHBOARD { 'S', 'C', 'A', 'L', '2' }
#define WIRELESS_TX_INTERVAL_MS 200
#define WIRELESS_LINK_TIMEOUT_MS 2000
```

The scale sends a 16-byte `WeightPacket` (magic, version, unit, flags,
sequence, grams, converted value) every `WIRELESS_TX_INTERVAL_MS`, with
auto-acknowledge on. The magic and version bytes let the receiver reject
traffic from any other nRF24 installation that happens to share the channel.

### Four files are shared with the dashboard by hand

The two firmwares are independent Arduino sketches and cannot share a
directory, so these are kept **byte-for-byte identical** by copying, and each
carries a banner saying so:

```
src/protocol/WeightPacket.h     the wire contract
src/drivers/SpiBus.*            shared-bus bring-up
src/drivers/MAX7219.*           display driver
src/drivers/NRF24.*             radio driver
```

The radio settings in `hardware.h` must match too — channel, data rate,
addresses, address width, payload size. Nothing checks this for you. One
thing *is* checked: a `static_assert` in `WirelessManager.cpp` fires if
`UnitManager::Unit` is ever reordered out of step with `PacketUnit`, because
that would quietly make the dashboard label kilograms as pounds.

**Link state** is deliberately three-valued, and the distinction survives all
the way to the display:

| State | Meaning |
|---|---|
| `NoRadio` | nothing answering on SPI — a wiring or supply fault on *this* board |
| `Down` | the radio works, the dashboard is off or out of range |
| `Up` | a transmission was acknowledged recently |

The scale proves the link with the auto-acknowledge rather than with "we sent
something" — an ack means the dashboard is powered, in range *and* listening
on the right address.

`WirelessManager` also re-probes a radio that has stopped answering every 5 s
and re-runs `begin()` if it comes back, which recovers the common case of a
brown-out resetting the module without resetting the ESP32.

For bring-up, `WirelessManager::logStatus(Serial)` dumps channel, data rate,
PA level, both addresses, STATUS/FIFO, lost-packet and retransmit counters
and the last error. It runs once at boot when `DEBUG_SERIAL` is on.

---

## Design Notes

**The loop never blocks.** The ADS1232 runs at 10 SPS, so a blocking read
would freeze the UI for 100 ms at a time. Instead `WeightManager::update()`
polls the data-ready line and only clocks out bits when a conversion is
actually waiting, and `MatrixKeypad` debounces by timestamp rather than
`delay()`. The one deliberate exception is the calibration capture — a clean
16-sample average matters more than responsiveness during those two seconds.

**Stability is measured in ADC counts, not grams.** The calibration procedure
needs stability detection, so it cannot depend on the calibration factor.

**Screen layout lives in the display driver.** A 6-digit LED module cannot
render "a title and two lines of instructions", so what is shared across
displays is the *semantic* call (`showWeight`, `showCalibrationStep`), which
is `DisplayManager`'s unified interface. Adding the MAX7219 meant writing one
driver and nothing above it changed — `DisplayManager` is now short enough
that most of its methods are a one-line forward.

**The dashboard is a separate firmware, and the seam is the packet.** This
project transmits and never decodes; `../WirelessDashboard` decodes and never
measures. What keeps them compatible is one hand-copied header with a version
byte in it, so a half-updated pair fails loudly (rising `rejected` count)
rather than quietly showing a wrong number.

**Flash endurance.** `StorageManager::save()` compares against the cached
record and skips the write when nothing changed, so booting and cycling units
does not burn erase cycles.

---

## Tuning

| Constant | File | Effect |
|---|---|---|
| `FILTER_WINDOW_SIZE` | hardware.h | larger = steadier, slower (8 ≈ 0.8 s at 10 SPS) |
| `STABILITY_THRESHOLD_COUNTS` | hardware.h | set from the observed at-rest jitter in the serial log |
| `ADC_SPEED_80SPS` | hardware.h | 1 = faster response, noisier (ADS1232 only) |
| `CAL_AVERAGE_SAMPLES` | calibration.h | more = better calibration, longer wait |
| `CAL_MIN_SPAN_COUNTS` | calibration.h | raise it if a bad calibration ever slips through |

---

## Bringing Up a Wireless Pair

1. Flash the **scale** (the radio is on by default). At 115200 baud the boot
   log should show the register dump with `connected : yes`. If it says `NO`,
   stop — that is wiring or supply, not radio configuration.
2. With no dashboard powered, the scale should log `[Wireless] TX failed: no
   ack (max retries)` once, then keep weighing normally and tag its log lines
   `[no link]`.
3. Flash the **dashboard** from `../WirelessDashboard`. See that project's
   README for the rest of the procedure.

If the two never link while both report `connected : yes`, the usual causes
in order are: a missing 10 µF capacitor (see the power warnings), radio
settings that differ between the two `hardware.h` files, or a
`WeightPacket.h` that has drifted between the two projects.

---

## Still To Do

`WeightManager` supports 2 and 4 load cells but only the 1-cell pin set is
filled in. `DiagnosticManager` has no per-cell sensor fault yet — a dead cell
in a 4-cell platform currently shows up only as a wrong reading.
