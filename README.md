# S32K144W Automotive Capacitive Touch Sensing Algorithm Engine (CVD & TinyML)

[![Target: NXP S32K144W](https://img.shields.io/badge/Target-NXP%20S32K144W-005a9c.svg)](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/s32k-automotive-mcus/s32k1-microcontrollers-for-general-purpose:S32K1)
[![Core: Arm Cortex-M4F](https://img.shields.io/badge/Core-Arm%C2%AE%20Cortex%C2%AE--M4F%20%40%2048MHz-blue.svg)](https://www.arm.com/products/silicon-ip-cpu/cortex-m/cortex-m4)
[![RTD: 3.0.0 (AUTOSAR 4.7)](https://img.shields.io/badge/RTD-3.0.0%20(AUTOSAR%C2%AE%204.7)-orange.svg)](https://www.nxp.com)
[![Toolchain: GCC 10.2](https://img.shields.io/badge/Toolchain-GCC%2010.2%20(0%20errors%20%7C%200%20warns)-brightgreen.svg)](https://gcc.gnu.org)
[![Functional Safety: ISO 26262 / ASIL-B Ready](https://img.shields.io/badge/Safety-ISO%2026262%20%2F%20ASIL--B%20Ready-success.svg)](https://www.iso.org/standard/68383.html)
[![EMC: IEC 61000-4-6 / CISPR 25](https://img.shields.io/badge/EMC-Spread--Spectrum%20MFP-purple.svg)](https://www.iec.ch)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

English | [繁體中文](README_zh.md)

---

Production-grade automotive capacitive touch sensing firmware engine designed for the **NXP Semiconductors® S32K144W evaluation board (`XS32K14WEVB-Q064`)**.

Implemented in strict adherence to **NXP Real Time Drivers (RTD 3.0.0)**, **S32 Configuration Tools (MEX)**, and **ISO 26262 / MISRA C** functional safety guidelines. Structured into **6 Progressive Engineering Milestones (M1 to M6)** on a dual-pin charge transfer CVD topology (`PTC17` driving, `R525` $4.7\text{ k}\Omega$, `PTC16/ADC0_SE14` sensing, `C533` $10\text{ pF}$, and $15\text{ mm} \times 15\text{ mm}$ copper electrode `SW7`).

---

## Key Features

* **Bipolar Four-Phase Fully Differential CVD**: Alternate positive and negative precharge & charge-sharing yielding $2\times$ differential physical signal gain ($\Delta D = V_B - V_A = 80 \sim 200\text{ LSB}$).
* **Deterministic Microsecond Physical Timing**: Synchronous High-Z switching via atomic register operations and jitter-free NOP balancing delays ($< 15\text{ LSB}$ error across $R_{525}$).
* **ADC0 32x Hardware Self-Calibration (AN12217)**: Automated power-on calibration with internal 1.0V Bandgap (Channel 27) $V_{\text{REFH}}$ monitoring.
* **Dual-Track Observability**: Real-time ASCII CSV telemetry at 115200 bps via OpenSDA virtual serial port and FreeMASTER TSA driver ready.
* **Visual Status Indication**: Tri-color RGB LED feedback for calibration pass (Green), active touch (Blue), and system faults (Red).

---

## Hardware Pinout (XS32K14WEVB-Q064)

| Signal / Role | MCU Port | 64-LQFP Pin | External Components | Description |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (Drive)** | `PTC17` | Pin 27 | $R_{525}$ ($4.7\text{ k}\Omega$), $R_{172}$ ($0\Omega$), `SW7` | Charge transfer drive / `ADC0_SE15` |
| **TOUCH_B (Sense)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ to GND), $R_{525}$ ($4.7\text{ k}\Omega$) | Charge transfer sense / `ADC0_SE14` |
| **Touch Pad** | `SW7` | Onboard PCB | $15\text{ mm} \times 15\text{ mm}$ Copper electrode | Sensing capacitance $C_x$ |
| **UART_TX** | `PTC7` | Pin 51 | OpenSDA Virtual COM TX (ALT2) | 115200 bps 8N1 Telemetry |
| **UART_RX** | `PTC6` | Pin 52 | OpenSDA Virtual COM RX (ALT2) | Serial receive channel |
| **RED LED** | `PTE7` | Pin 39 | GPIO Output | Fault indicator (Active High) |
| **GREEN LED** | `PTE0` | Pin 60 | GPIO Output | Ready / Idle baseline indicator |
| **BLUE LED** | `PTE3` | Pin 13 | GPIO Output | Touch detected indicator |

---

## Milestone Progress Roadmap

- [x] **Milestone 1: Bipolar Differential Physical Layer & 2x Gain Validation** (DONE)
  - ADC0 32x self-calibration & Bandgap reading
  - Microsecond bipolar CVD acquisition (`Touch_AcquireSingleDiffCVD`)
  - Untouched workpoint $1600 \sim 2500\text{ LSB}$, balance diff $< 15\text{ LSB}$, touch delta $80 \sim 200\text{ LSB}$
- [ ] **Milestone 2: Spread-Spectrum Hopping & Median-Cascaded Kalman Filter**
- [ ] **Milestone 3: Asymmetric Baseline Tracker with Touch-on-Boot Recovery**
- [ ] **Milestone 4: Temporal Envelope Feature Classification (TinyML) & Water Rejection**
- [ ] **Milestone 5: Online Dual-Channel Hardware Defect Diagnostics**
- [ ] **Milestone 6: VLPS Low-Power Sleep/Wakeup Integration & 12h Burn-in Test**

---

## Build and Flashing Instructions

```powershell
# 1. Setup S32DS toolchain environment
$env:PATH = "C:\NXP\S32DS.3.6.11\S32DS\build_tools\msys32\usr\bin;C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin;" + $env:PATH

# 2. Compile via GNU Make (0 errors, 0 warnings)
cd Debug_FLASH
make -j8 all

# 3. Flash to target board via OpenSDA PEMicro
python ../tools/flash_target.py

# 4. Stream 100-sample verification dataset
python ../tools/capture_telemetry.py COM5
```

---

## License

BSD-3-Clause License. Copyright (c) 2026, Max97k.
