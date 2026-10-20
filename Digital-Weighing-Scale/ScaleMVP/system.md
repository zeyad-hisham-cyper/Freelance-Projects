# Universal Weighing Controller

## Overview

The Universal Weighing Controller is a modular and configurable industrial weighing platform designed around the ESP32 DevKit. The project aims to provide a single firmware that can support multiple weighing applications without requiring code modifications for each scale.

The controller supports different load cell capacities, multiple load cells, interchangeable display types, wireless communication, and easy calibration. Most hardware differences are handled through compile-time configuration using preprocessor directives, allowing the same codebase to be reused across different weighing systems.

The design philosophy is:

> One firmware, many weighing systems.

---

# Objectives

- Support load cells from a few grams up to approximately 10 tons.
- Support one or multiple load cells.
- Support different display types.
- Support different ADC modules.
- Allow easy calibration after changing the load cell.
- Keep the firmware modular and easy to maintain.
- Minimize Flash and RAM usage using compile-time configuration.
- Build everything using Arduino IDE.

---

# Hardware

## Main Controller

- ESP32 DevKit

---

## Supported ADCs

- ADS1232 (Preferred)
- HX711

Each load cell has its own dedicated ADC.

The ADC converts the Wheatstone Bridge signal into digital data before sending it to the ESP32.

This significantly improves noise immunity over long cable runs.

---

## Load Cells

The controller supports:

- Single Load Cell
- Two Load Cells
- Four Load Cells
- Future expansion

Every load cell is connected to its own ADC.

The firmware reads every ADC independently.

Changing the load cell only requires recalibration.

No firmware modification should be necessary.

Supported capacity:

Approximately

- 100 g
- 500 g
- 1 kg
- 5 kg
- 20 kg
- 50 kg
- 100 kg
- 500 kg
- 1 ton
- 5 tons
- 10 tons

or any compatible load cell.

---

## ADC Wiring

Load Cell

↓

ADC

↓

6-meter Shielded Cable

↓

ESP32

Since communication is digital after the ADC, long-distance communication is much more reliable.

---

# Displays

The controller supports two display options.

## Option 1

6-Digit Seven Segment Display

Driver:

MAX7219

Suitable for industrial indicators.

---

## Option 2

OLED Display

Suitable for:

- Debugging
- Future expansion
- Additional information

---

Display selection is performed through:

- Jumper
- Bridge Connector
- Compile-time Configuration

Only one display is enabled in the firmware.

---

# Wireless Communication

Communication module:

NRF24L01

Purpose:

Transmit weight wirelessly to another display board.

---

# Wireless Dashboard

Remote board consists of:

- ESP32
- NRF24L01
- MAX7219
- Six Digit Seven Segment Display

The dashboard receives the measured weight and displays it in real time.

---

# User Interface

The controller includes three dedicated push buttons.

## TARE Button

When pressed:

- Read the current measured weight.
- Store it as the tare offset.
- Reset the displayed weight to zero.

This function allows containers or fixtures to be ignored during measurement.

---

## MODE Button

Every press cycles through the available weight units.

Sequence:

Gram

↓

Kilogram

↓

Pound (lb)

↓

Ounce (oz)

↓

Back to Gram

Only the displayed value changes.

The internal measurement remains unchanged.

---

## CALIBRATION Button

Pressing this button starts the calibration process.

Calibration sequence:

1. Remove all weight.
2. Capture zero offset.
3. Place a known calibration weight.
4. Enter the calibration weight.
5. Calculate scale factor.
6. Store calibration data.
7. Return to normal operation.

---

# Measurement Flow

Power ON

↓

Load Configuration

↓

Initialize Hardware

↓

Load Calibration Data

↓

Read ADC

↓

Digital Filtering

↓

Average Multiple Load Cells (if enabled)

↓

Apply Calibration

↓

Apply Tare

↓

Convert Unit

↓

Update Display

↓

Transmit Wireless Data

↓

Repeat

---

# Software Architecture

```
UniversalWeighingController/

│

├── config/

│   ├── config.h

│   ├── pins.h

│   ├── features.h

│   ├── hardware.h

│   └── calibration.h

│

├── drivers/

│   ├── ADS1232

│   ├── HX711

│   ├── MAX7219

│   ├── OLED

│   ├── NRF24

│   └── EEPROM

│

├── modules/

│   ├── WeightManager

│   ├── CalibrationManager

│   ├── DisplayManager

│   ├── WirelessManager

│   ├── UnitManager

│   ├── StorageManager

│   ├── FilterManager

│   └── DiagnosticManager

│

├── app/

│   ├── setup.cpp

│   ├── loop.cpp

│   └── main.ino

│

└── README.md
```

---

# Configuration System

All project customization is done inside the configuration files.

The application logic never needs modification.

Example:

```cpp
#define SCALE_CAPACITY_KG      10000

#define LOAD_CELL_COUNT        4

#define USE_ADS1232

//#define USE_HX711

#define DISPLAY_7SEG

//#define DISPLAY_OLED

#define ENABLE_NRF

#define ENABLE_EEPROM

#define ENABLE_FILTER

//#define DEBUG_SERIAL
```

---

# Pin Configuration

All GPIO assignments are stored in:

```
pins.h
```

Examples include:

- ADC Pins
- Display Pins
- NRF Pins
- Button Pins

Changing the ESP32 board should only require editing this file.

---

# Compile-Time Features

The firmware uses preprocessor directives to remove unused code.

Example:

```cpp
#ifdef DISPLAY_OLED

// OLED Driver

#endif
```

If OLED is disabled, its driver is not compiled.

Benefits:

- Smaller Flash usage
- Lower RAM usage
- Faster execution
- Easier maintenance

---

# Software Modules

## Weight Manager

Responsible for:

- Reading ADC values
- Combining multiple load cells
- Returning raw measurements

---

## Calibration Manager

Responsible for:

- Zero Offset
- Scale Factor
- Calibration Procedure
- EEPROM Storage

---

## Display Manager

Provides a unified interface.

Example:

```cpp
DisplayManager.showWeight();
```

The rest of the firmware does not know whether the display is OLED or Seven Segment.

---

## Wireless Manager

Responsible for:

- NRF Initialization
- Packet Transmission
- Packet Reception
- Connection Status

---

## Unit Manager

Responsible for converting weight units.

Supported units:

- Gram
- Kilogram
- Pound (lb)
- Ounce (oz)

The internal measurement remains unchanged.

Only the displayed value is converted.

---

## Storage Manager

Responsible for:

- Saving calibration
- Loading calibration
- Saving user settings

---

## Filter Manager

Responsible for:

- Noise Reduction
- Stable Measurements
- Digital Filtering

Future filters can be added without changing other modules.

---

## Diagnostic Manager

Responsible for:

- ADC Errors
- Wireless Errors
- Sensor Errors
- Hardware Status

---

# Design Goals

The firmware should be:

- Modular
- Reusable
- Configurable
- Lightweight
- Well documented
- Easy to maintain
- Easy to expand
- Suitable for industrial applications

---

# Future Features

Possible future additions include:

- RS-232
- RS-485
- Modbus RTU
- Ethernet
- Wi-Fi Dashboard
- Bluetooth
- Data Logging
- SD Card Support
- Cloud Connectivity
- Relay Outputs
- Analog Output (4–20 mA / 0–10 V)
- Web Configuration
- OTA Firmware Updates
- Password-Protected Calibration

---

# Design Philosophy

The Universal Weighing Controller is designed to be a reusable firmware platform rather than a single-purpose application. By separating hardware-specific code into drivers, organizing functionality into independent modules, and controlling features through compile-time configuration, the same firmware can be deployed across a wide range of weighing systems with minimal effort. Replacing a load cell, changing the display type, or selecting a different ADC should require only configuration changes and recalibration, without modifying the core application logic. This approach improves maintainability, scalability, memory efficiency, and long-term reliability while keeping the codebase clean, modular, and easy to extend.