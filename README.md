# Freelance Projects

A portfolio of embedded systems, PCB design and IoT projects delivered for freelance clients and events, from schematic and PCB layout through firmware to companion mobile apps.

**Core skills:** ESP32 · Arduino / PlatformIO (C++) · KiCad 9 PCB design · sensor integration (IMU, IR temperature, load cells, PPG) · BLE · ESP-NOW · nRF24L01+ · Flutter · Li-Po power design

---

## Projects

| | Project | What it is | Hardware | Firmware / Software |
|---|---|---|:---:|:---:|
| <img src="Smart-Posture-Corrector/docs/images/main_board/pcb_3d_iso.png" width="180"> | **[Smart Posture Corrector](Smart-Posture-Corrector/)** | Two-board wearable with two IMUs that vibrates when you slouch and streams posture data to a Flutter app over BLE. | ✅ KiCad (2 boards) | ✅ ESP32 firmware + Flutter app |
| <img src="Digital-Weighing-Scale/docs/images/research-system-architecture.png" width="180"> | **[Digital Weighing Scale](Digital-Weighing-Scale/)** | Load-cell weighing indicator built in stages: research, ESP-NOW prototype, then a modular "one firmware, many scales" controller with a wireless remote display. | Research + BOM | ✅ Arduino / ESP32 firmware, Nextion HMI |
| <img src="Cow-Health-Monitor/docs/images/pcb_3d_iso.png" width="180"> | **[Cow Health Monitor](Cow-Health-Monitor/)** | ESP32 carrier board plus two remote sensor boards for a livestock collar: body temperature, heart rate / SpO₂, motion and GPS. | ✅ KiCad (1- and 2-layer variants) | — |
| <img src="Smart-Emergency-Dispatch-Sensor/docs/images/pcb_3d_iso.png" width="180"> | **[Smart Emergency Dispatch Sensor](Smart-Emergency-Dispatch-Sensor/)** | ESP32 board with an IR thermometer, a 6-axis IMU, a GPS header and a buzzer/LED alarm for heat, fire or impact events. | ✅ KiCad, 2-layer THT | — |
| <img src="TelecomVillage-SIM-Adapter-Badge/docs/images/pcb_3d_iso.png" width="180"> | **[Telecom Village SIM Adapter Badge](TelecomVillage-SIM-Adapter-Badge/)** | Credit-card-sized conference badge that doubles as a 2FF / 3FF / 4FF smart-card adapter, with ISO 7816 contacts broken out to test pads. | ✅ KiCad, SMT-assembly ready | — |

Each project folder has its own README with an overview, block diagram, BOM, pin mapping, PCB specifications, build instructions and a render gallery.

---

## Repository layout

```
Freelance-Projects/
├── Smart-Posture-Corrector/           # ESP32 firmware (PlatformIO) + Flutter app + KiCad PCBs
├── Digital-Weighing-Scale/            # Load-cell firmware (MVP + v2), Nextion HMI, research figures
├── Cow-Health-Monitor/                # KiCad PCB set + vendor 3D models
├── Smart-Emergency-Dispatch-Sensor/   # KiCad PCB + Gerbers
├── TelecomVillage-SIM-Adapter-Badge/  # KiCad PCB, Gerbers, pick-and-place, badge artwork
└── kicad-shared-lib/                  # Symbols, footprints and 3D models shared by several boards
```

Every hardware project follows the same pattern:

- **KiCad source:** `*.kicad_pro`, `*.kicad_sch`, `*.kicad_pcb`
- **Fabrication outputs:** Gerber and drill files, plus a zip ready to upload to JLCPCB / PCBWay
- **`docs/`:** 3D renders, PCB layout, schematic (SVG + PDF)

## Opening the KiCad projects

1. Install **KiCad 9.x**.
2. Clone the whole repository. Some boards load 3D models from `kicad-shared-lib/` or a sibling project through `${KIPRJMOD}`-relative paths, so the folder structure must stay intact.
3. Open the `.kicad_pro` file in the project folder.

## Regenerating the figures

The renders in each `docs/images/` folder were produced with `kicad-cli`:

```bash
kicad-cli pcb render --quality high --width 1600 --height 1000 --rotate " -45,0,35" -o docs/images/pcb_3d_iso.png board.kicad_pcb
kicad-cli pcb render --side top    --quality high -o docs/images/pcb_3d_top.png    board.kicad_pcb
kicad-cli pcb render --side bottom --quality high -o docs/images/pcb_3d_bottom.png board.kicad_pcb
kicad-cli pcb export svg --layers "F.Cu,B.Cu,Edge.Cuts,F.Silkscreen" --mode-single --page-size-mode 2 --exclude-drawing-sheet -o docs/images/pcb_layout.svg board.kicad_pcb
kicad-cli sch export svg -o docs/images/schematic board.kicad_sch
kicad-cli sch export pdf -o docs/schematic.pdf board.kicad_sch
```

## Third-party content

- Vendor symbols, footprints and 3D models (SnapEDA, DFRobot, CUI, GCT and others) remain under their owners' licences.
- The 3D-printable load-cell frame in `Digital-Weighing-Scale/` is © indrekluuk ([Thingiverse 4602226](https://www.thingiverse.com/thing:4602226)), licensed CC BY-NC.
- Event and university logos on the silkscreens belong to their respective owners.

---

**Author:** [@zeyad-hisham-cyper](https://github.com/zeyad-hisham-cyper) — Embedded Systems & PCB Design
