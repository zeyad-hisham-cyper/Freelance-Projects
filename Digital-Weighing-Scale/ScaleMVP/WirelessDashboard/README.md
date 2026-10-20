# Wireless Dashboard

ESP32 firmware for a remote weight display. It listens on an nRF24L01+ link
and shows whatever the **Universal Weighing Controller** reports on a MAX7219
six-digit seven-segment panel.

It has no load cell, no calibration and no buttons. Its one job is to be
trustworthy: **a remote display must never show a number that stopped being
true.** Every design decision below follows from that.

---

## Hardware / Pin Map

All GPIO assignments live in `src/config/pins.h` — nothing else in the
firmware references a pin number.

| Function | Signal | GPIO |
|---|---|---|
| **SPI bus** (shared) | SCK  | 18 |
| **SPI bus** (shared) | MOSI | 23 |
| **SPI bus** (shared) | MISO | 19 |
| MAX7219 | DIN → MOSI | 23 |
| MAX7219 | CLK → SCK  | 18 |
| MAX7219 | CS   | 5  |
| nRF24L01+ | CE  | 4  |
| nRF24L01+ | CSN | 15 |
| nRF24L01+ | IRQ | not connected (the driver polls) |

Power: MAX7219 from **5 V**, nRF24L01+ from **3.3 V** (5 V destroys it),
common ground.

The radio pins differ from the scale controller's (GPIO16/GPIO17) — the two
boards are separate products with separate layouts. Only the SPI bus lines
and the radio *settings* have to match, never the GPIOs.

**GPIO15 is a strapping pin** (MTDO): held LOW at reset it silences the boot
ROM log on U0TXD. Harmless here, since CSN idles HIGH and the driver drives
it HIGH before the bus comes up — but if this board ever loses its boot log,
that pin is the first suspect.

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
  3.3 V regulators cannot deliver cleanly. A receiver still transmits
  acknowledges, so this bites here too: if the radio reports `connected :
  yes` but nothing is ever received, solder a 10 µF capacitor directly across
  the module's VCC/GND pins. **Never feed its VCC from 5 V.**

---

## Two Independent Firmwares

This is a standalone Arduino sketch. Its counterpart, the scale, lives in
`../UniversalWeighingController`. They share nothing at build time — but
**four files are kept byte-for-byte identical by hand**, and the link breaks
silently if they drift:

```
src/protocol/WeightPacket.h     the wire contract
src/drivers/SpiBus.*            shared-bus bring-up
src/drivers/MAX7219.*           display driver
src/drivers/NRF24.*             radio driver
```

Each of those files carries a banner saying so. Diff them whenever the two
boards stop understanding each other:

```
fc src\protocol\WeightPacket.h ..\UniversalWeighingController\src\protocol\WeightPacket.h
```

The **radio settings** in `src/config/hardware.h` must match the scale's too
— channel, data rate, addresses, address width, payload size. Nothing checks
this for you, and a mismatch is the most confusing failure this system can
produce: both boards report a perfectly healthy radio and never exchange a
packet.

---

## Building

Arduino IDE → open `WirelessDashboard/WirelessDashboard.ino` → board **ESP32
Dev Module**.

**No libraries required.** Both drivers are register-level and depend on
nothing but the ESP32 core's SPI library.

See *Verification* at the end of this file for what has and has not been
checked on real hardware.

---

## Project Layout

The structure mirrors the scale's, with `config/ drivers/ modules/ app/`
inside `src/` — the Arduino IDE only compiles sub-folders that live there.

```
WirelessDashboard/
├── WirelessDashboard.ino           entry point (delegates to App)
└── src/
    ├── config/
    │   ├── config.h        includes everything below, sanity checks
    │   ├── features.h      diagnostics, serial logging
    │   ├── hardware.h      digits, brightness, radio, link timeout
    │   └── pins.h          every GPIO
    ├── protocol/
    │   └── WeightPacket.h  the wire contract (shared with the scale)
    ├── drivers/
    │   ├── SpiBus.*        one-time bring-up of the shared SPI bus
    │   ├── MAX7219.*       registers, segment font, screen layouts
    │   └── NRF24.*         register-level nRF24L01+ transceiver
    ├── modules/
    │   ├── DisplayManager.*     packet/link state -> panel screens
    │   ├── WirelessManager.*    reception, validation, link state
    │   ├── DiagnosticManager.*  may we show a number, and if not, why not
    │   └── UnitFormat.h         unit -> label and decimals
    └── app/
        ├── App.h
        ├── setup.cpp       power-on sequence
        └── loop.cpp        main cycle
```

### Layering rule

`app → modules → drivers → hardware`, never upward and never sideways. The
whole cycle is four steps:

```
receive -> validate -> classify -> display
```

`WirelessManager` stops at **a validated packet and its age**;
`DiagnosticManager` turns that into **may we show a number**; `DisplayManager`
turns a packet into **screens**. Nothing else decides what is safe to show.

---

## Configuration

Everything customisable is in `src/config/`. The application logic never
needs editing.

```cpp
// hardware.h — display
#define MAX7219_DIGITS 6
#define MAX7219_BRIGHTNESS 12           // 0..15; a dashboard is read from far away
#define MAX7219_DIGIT0_IS_RIGHTMOST 0   // measured: DIG0 is the LEFTMOST digit here
#define MAX7219_SPI_HZ 1000000          // low on purpose — see Troubleshooting
#define MAX7219_CONFIG_REFRESH_MS 1000
#define DISPLAY_SELFTEST_ON_BOOT 0      // off so "all segments lit" has one meaning

// hardware.h — radio (MUST MATCH THE SCALE)
#define NRF_CHANNEL 76         // 2476 MHz — inside the 2.4 GHz ISM band (0..83)
#define NRF_DATA_RATE 0        // 0 = 1 Mbps, 1 = 2 Mbps, 2 = 250 kbps
#define NRF_PA_LEVEL 3         // 0 = -18 dBm ... 3 = 0 dBm
#define NRF_ADDRESS_SCALE     { 'S', 'C', 'A', 'L', '1' }
#define NRF_ADDRESS_DASHBOARD { 'S', 'C', 'A', 'L', '2' }
#define NRF_PAYLOAD_SIZE 20

// hardware.h — the most important number in this file
#define LINK_TIMEOUT_MS 2000   // older than this and the reading is not shown
```

`config.h` refuses to build a configuration that cannot work: a digit count
above 8, a channel above 125, a link timeout shorter than the redraw
interval. Every one of those is a `#error` with the fix in the message.

---

## What the Panel Shows

Six digits, no annunciator LEDs. The driver runs the MAX7219 in **no-decode**
mode with its own segment font, because the chip's BCD decoder can only draw
`0-9 - E H L P` and this panel has to write `Err` and `------` too.

| Screen | Meaning |
|---|---|
| every segment lit | **not a screen** — see Troubleshooting |
| `------` | powered up, nothing heard from the scale yet |
| `  2.500` | a fresh reading, in the unit the scale is displaying |
| leftmost decimal point lit | NET (the scale is tared) |
| `  OL  ` | the value will not fit in six digits even at zero decimals |
| `Err rA` | no nRF24 answering on the SPI bus — wiring or supply on **this** board |
| `Err no` | radio fine, no packet within `LINK_TIMEOUT_MS` — the scale is off, out of range, or configured differently |
| `Err Sc` | packets arriving, and the scale is reporting its own hardware fault |

Decimals follow the unit, matching the scale exactly: `g` 1, `kg` 3, `lb` 3,
`oz` 2. That is how the operator tells `2.500 kg` from `2500.0 g` on a panel
that cannot print the unit itself.

**Stability is not shown.** A bare six-digit panel has one spare element —
the leftmost decimal point — and NET is the more important thing to spend it
on. The flag is carried in the packet, so a panel with an annunciator LED
could show it without touching the protocol.

A boot self-test — lamp test, walking segment, 0-9 count — is available via
`DISPLAY_SELFTEST_ON_BOOT` and is the only way to prove every segment of a
write-only panel. It ships **off**, so that all-segments-lit means exactly
one thing; see Troubleshooting.

---

## The Link

The scale sends a 16-byte `WeightPacket` — magic, version, unit, flags,
sequence, grams, converted value — every 200 ms with auto-acknowledge on.
This board listens, and its acknowledge is what tells the scale the link is
alive.

**A packet is only accepted if its magic and version match.** Another nRF24
installation sharing the channel, or a scale running a different protocol
version, is counted as `rejected` and never decoded as a weight.

**Link state is three-valued**, and the distinction survives to the panel:

| State | Panel | Meaning |
|---|---|---|
| `NoRadio` | `Err rA` | nothing answering on SPI — a fault on *this* board |
| `Down` | `Err no` | the radio works; the scale is off, out of range or silent |
| `Up` | the reading | a valid packet arrived within `LINK_TIMEOUT_MS` |

`WirelessManager` also counts **missed** packets from the gaps in the
sequence numbers — the only trace a lost packet leaves. A steady trickle is
normal at the edge of range; a rising rate means interference or a marginal
supply. It re-probes a radio that has stopped answering every 5 s and re-runs
`begin()` if it comes back, which recovers the common case of a brown-out
resetting the module without resetting the ESP32.

---

## Bringing Up a Pair

1. Flash the **scale**. `ENABLE_NRF` is on by default there; check its boot
   log for `connected : yes`.
2. Flash this **dashboard**. Its boot log dumps the same register summary. If
   it says `connected : NO`, stop — that is wiring or supply, not radio
   configuration.
3. With the scale off, the dashboard should show `Err no` (not a stale
   number, and not `0`).
4. Power both. The dashboard follows the scale's reading, unit and NET state
   within a redraw interval.

If step 4 never happens while both boards report `connected : yes`, the usual
causes in order are:

1. the 10 µF capacitor is missing on one of the modules;
2. `NRF_CHANNEL`, the addresses, the data rate or `NRF_PAYLOAD_SIZE` differ
   between the two `hardware.h` files;
3. `WeightPacket.h` has drifted between the two projects — `fc` them;
4. only one of the two boards was re-flashed after a protocol change (the
   version byte makes this show up as a rising `rejected` count rather than
   as garbage).

### Packets get through, but only some of them

Different problem, and a more common one. The scale's log tells you which
you have: `tx=` shows sent/failed counters on every measurement line, and
`[link]` / `[no link]` shows whether an ack arrived in the last 2 s. A link
that reads `[link]` while `tx=` climbs on both sides is a *marginal* link,
not a misconfigured one — addresses and channel are already proven correct
by the acks that do arrive.

In the order worth trying:

1. **Data rate.** Many modules sold as nRF24L01+ are Si24R1 clones whose
   250 kbps mode is unreliable. `NRF_DATA_RATE 0` (1 Mbps) works on every
   module ever made and is now the default on both boards. A mixed pair —
   one genuine, one clone — at 250 kbps fails exactly like a weak signal.
2. **Decoupling.** 10 µF (47-100 µF is better) plus 100 nF soldered directly
   across each module's VCC/GND pins, not on a breadboard rail.
3. **Distance, if they are on the same desk.** At 0 dBm two modules under a
   metre apart can overload each other's receivers. Drop `NRF_PA_LEVEL` to
   0 or 1 and see whether it *improves*; if it does, that was the cause and
   the real installation at range will want the power back.
4. **Channel.** `NRF_CHANNEL 76` (2476 MHz) is the default. Anything above
   83 is outside the 2.4 GHz ISM band — illegal to radiate in and outside
   the antenna's tuning. If WiFi is heavy, sweep a few channels in 0..83
   rather than climbing out of the band.
5. **Antenna orientation and metal.** A module lying flat against a metal
   enclosure or a battery pack loses far more than the datasheet suggests.

With `DEBUG_SERIAL` on, the dashboard prints value, unit, packet age, and the
received / missed / rejected counters once a second at 115200 baud.

---

## Troubleshooting the Panel

### The number reads backwards — `123456` shows as `654321`

`MAX7219_DIGIT0_IS_RIGHTMOST` in `hardware.h` describes which physical digit
the chip's DIG0 output drives, and it is a property of the module's PCB, not
of the chip. **This is already set to 0 for the module in use here.** If a
replacement panel ever comes out mirrored again, flip it back to 1 — nothing
else in the firmware needs to change, because every position in the driver
API is counted from the left of the panel and the define does the
translation.

### The panel works for a while, then lights every segment and stays that way

That is the MAX7219's **display-test register** (0x0F) being written with a
non-zero data byte by a corrupted word on the SPI bus. Display test overrides
the digit registers, so redrawing the value does not clear it — only a power
cycle did, before the fixes below.

Three changes address it:

| Change | Effect |
|---|---|
| `MAX7219_SPI_HZ` 8 MHz → **1 MHz** | far fewer corrupted words to begin with; still ~100 µs to redraw the panel |
| `MAX7219_CONFIG_REFRESH_MS 1000` | the driver re-sends every control register once a second, so a corruption clears itself within ~1 s instead of never |
| `SpiBus::begin()` deselects **both** devices before `SPI.begin()` | closes a real gap: the radio's CSN was an undriven input while the display was being configured, and vice versa. Neither line has a pull-up, so the idle device could latch its neighbour's traffic |

**These are a net, not a cure.** The underlying problem is that the MAX7219
runs at 5 V and specifies `V_IH` = 3.5 V, while the ESP32 drives 3.3 V — out
of spec, and marginal enough that long jumper leads tip it over. If the
symptom persists at 1 MHz, fix the levels: a 74AHCT125 on DIN/CLK/CS, or run
the module at 3.3 V and accept dimmer digits. Shorten the leads and add 100 nF
across the module's supply while you are there.

The same corruption can land on the shutdown or scan-limit register instead,
where it shows up as a panel frozen on its last value or with digits missing.
The once-a-second refresh covers those too.

### The whole panel lights up briefly at power-up

That is the boot self-test, and it is now **off by default**
(`DISPLAY_SELFTEST_ON_BOOT 0`) precisely so it cannot be confused with the
fault above. If you see all segments lit at boot with it off, the board is
resetting repeatedly — suspect the supply, not the display.

Turn it on for bench work: it is still the only way to prove every segment of
a write-only panel, and the quickest way to check the digit order.

---

## Design Notes

**The fault check outranks the reading, always.** `refreshDisplay()` asks
`DiagnosticManager` first and returns early on any fault, so there is no code
path that reaches `showReading()` with data the diagnostics have rejected.
That ordering is the entire safety argument of this firmware; keep it if you
edit `loop.cpp`.

**`------`, not `0`, before the first packet.** A dashboard showing `0`
before it has heard anything is indistinguishable from an empty platform.

**No conversion happens here.** The packet carries both the canonical grams
and the value already converted to the scale's display unit, so the two
panels can never disagree because of a rounding difference. This board only
needs to know how to *label* a unit and how many decimals it implies —
`UnitFormat.h`, which must match the scale's `UnitManager`.

**The receiver validates; it does not trust.** Magic, version and unit range
are all checked. An unknown unit falls back to grams rather than indexing off
the end of a table.

**Why a register-level nRF24 driver instead of the RF24 library.** The same
reason the scale bit-bangs its ADCs: every timing rule and every register
write stays visible in the repository. The chip has 24 registers; a
3000-line dependency would be the only opaque box in either firmware.

---

## Verification

Both firmwares were syntax-checked with `g++ -fsyntax-only -Wall -Wextra`
against stub `Arduino.h` / `SPI.h`, and the MAX7219 layout logic (decimal-
point folding, right-alignment, decimal shedding, overflow, every fault tag)
was exercised on the host. `sizeof(WeightPacket)` is 16 bytes with no
padding, and the shared files were confirmed byte-identical across the two
projects.

Since then the firmware has been flashed to real hardware, which is where the
digit-order and display-test faults above were found and fixed. Those fixes
have **not** been re-verified on the bench yet.
