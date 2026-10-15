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

Implemented in strict adherence to **NXP Real Time Drivers (RTD 3.0.0)**, **S32 Configuration Tools (MEX)**, and **ISO 26262 / MISRA C** functional safety guidelines. Structured into **6 Progressive Engineering Milestones (M1 to M6)** on a dual-pin charge transfer CVD topology (`PTC17` driving, `R525` $10\text{ k}\Omega$ / $4.7\text{ k}\Omega$, `PTC16/ADC0_SE14` sensing, `C533` $10\text{ pF}$, and $15\text{ mm} \times 15\text{ mm}$ copper electrode `SW7`).

---

## Key Features

* **Bipolar Four-Phase Fully Differential CVD**: Alternate positive and negative precharge & charge-sharing yielding $2\times$ differential physical signal gain ($\Delta D = V_B - V_A = 80 \sim 200\text{ LSB}$).
* **Multi-Frequency Spread-Spectrum Burst Acquisition (MFP)**: 16-bit Galois LFSR (`0xB400`) with coprime delays $\{15, 23, 31\}$ cycles to break EMI harmonic lock.
* **Cascaded Non-Linear Filtering Engine**: Alpha-trimmed mean ($N=9$, discarding top/bottom 2 extreme outlier spikes) cascaded with 1D Adaptive Scalar Kalman Filter ($Q=0.35, R=1.0, \alpha=0.75$), achieving $\text{SNR} = 65.57\text{ dB}$ and residual noise $\sigma = 0.103\text{ LSB}$.
* **4-Zone Asymmetric Dynamic Baseline Tracker**: Slew-rate bounded ambient tracking ($+1.5\text{ LSB/s}$ / $-2.0\text{ LSB/s}$), approach freeze at $0.4\times\text{TH}$, 15-second foreign object hold timeout, and touch-on-boot negative self-healing in $\le 285\text{ ms}$.
* **TinyML Temporal Envelope Feature Classification**: 4 physical feature extractors (attack slope, dynamic SNR, plateau variance, energy) with 3-step confidence sliding window hysteresis ($100\%$ bare/glove detection, $0\%$ water/hover false triggers, $45\text{ ms}$ latency).
* **ARM Cortex-M SysTick Zero-Jitter Synchronizer**: Exact $15.000\text{ ms}$ hardware-locked frame period eliminating software loop stretching.
* **On-line Hardware Defect Diagnostics (ASIL-B Ready)**: Real-time monitoring of Short-to-GND ($< 300\text{ LSB}$), Short-to-VDD ($> 3800\text{ LSB}$), $R_{525}$ open circuit, and Bandgap $V_{\text{REFH}}$ drift ($\pm 5\%$) with $30\text{ ms}$ qualification and $120\text{ ms}$ self-healing auto-recovery.
* **Deterministic MISRA C Architecture**: Zero dynamic memory allocation (0 Bytes heap), bounded loops via timeout guards, static stack consumption $< 250\text{ Bytes}$.

---

## Hardware Pinout (XS32K14WEVB-Q064)

| Signal / Role | MCU Port | 64-LQFP Pin | External Components | Description |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (Drive)** | `PTC17` | Pin 27 | $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$), $R_{172}$ ($0\Omega$), `SW7` | Charge transfer drive / `ADC0_SE15` |
| **TOUCH_B (Sense)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ to GND), $R_{525}$ | Charge transfer sense / `ADC0_SE14` |
| **Touch Pad** | `SW7` | Onboard PCB | $15\text{ mm} \times 15\text{ mm}$ Copper electrode | Sensing capacitance $C_x$ |
| **UART_TX** | `PTC7` | Pin 51 | OpenSDA Virtual COM TX (ALT2) | 115200 bps 8N1 Telemetry |
| **UART_RX** | `PTC6` | Pin 52 | OpenSDA Virtual COM RX (ALT2) | Serial receive channel |
| **RED LED** | `PTE7` | Pin 39 | GPIO Output | Fault indicator / Fail-safe mode (Active High) |
| **GREEN LED** | `PTE0` | Pin 60 | GPIO Output | Ready / Idle baseline indicator (Active High) |
| **BLUE LED** | `PTE3` | Pin 13 | GPIO Output | Touch confirmed indicator (Active High) |

---

## Milestone Progress Roadmap

- [x] **Milestone 1: Bipolar Differential Physical Layer & 2x Gain Validation** (`v1.0.1-m1`)
  - [x] ADC0 32x hardware self-calibration (AN12217) & Bandgap (Channel 27) $V_{\text{REFH}}$ monitoring.
  - [x] Microsecond bipolar 4-phase CVD acquisition (`Touch_AcquireSingleDiffCVD`).
  - [x] Workpoint validation ($1600 \sim 2500\text{ LSB}$), balanced diff $< 15\text{ LSB}$, touch delta $80 \sim 200\text{ LSB}$.
  - [x] Zero heap, timeout guarded execution, 100-sample verification dataset (`m1_baseline_100_samples.csv`).
- [x] **Milestone 2: Spread-Spectrum Hopping & Median-Cascaded Kalman Filter** (`v1.0.2-m2`)
  - [x] 16-bit Galois LFSR (`0xB400`) with coprime hopping delays $\{15, 23, 31\}$ cycles.
  - [x] $N=9$ multi-frequency burst acquisition with alpha-trimmed mean (top/bottom 2 outlier rejection).
  - [x] 1D adaptive scalar Kalman filter ($Q=0.35, R=1.0, \alpha=0.75$), gain dynamically mapped from noise spread.
  - [x] Baseline noise $\sigma = 0.103\text{ LSB}$ ($5.86\times$ suppression), touch $\text{SNR} = 65.57\text{ dB}$, $+800\text{ LSB}$ EFT residual $< 0.33\text{ LSB}$.
- [x] **Milestone 3: Asymmetric Baseline Tracker with Touch-on-Boot Recovery** (`v1.0.3-m3`)
  - [x] 4-Zone asymmetric dynamic baseline tracking state machine (Tracking, Hold, Approach Freeze, Negative Recovery).
  - [x] Slew rate bounded ambient tracking ($+1.5\text{ LSB/s}$ / $-2.0\text{ LSB/s}$).
  - [x] Touch-on-boot negative self-healing in $285\text{ ms}$ (target $\le 300 \sim 500\text{ ms}$).
  - [x] 5-second ultra-slow approaching zero signal swallowing (freeze at $0.4\times\text{TH}$).
  - [x] 15-second foreign object continuous hold timeout auto-reset.
- [x] **Milestone 4: Temporal Envelope Feature Classification (TinyML) & Water Rejection** (`v1.0.4-m4`)
  - [x] 4 temporal envelope feature extractors: attack slope, dynamic SNR, plateau variance, energy.
  - [x] 3-step confidence sliding window hysteresis ($\text{trigger} = 0.65, \text{release} = 0.35$).
  - [x] Multi-class recognition: Bare Finger ($\text{TPR} = 100\%$), Glove/Acrylic ($\text{TPR} = 100\%$), Water Spray ($\text{FPR} = 0\%$), Edge Hover ($\text{FPR} = 0\%$).
  - [x] Detection latency: $45.0\text{ ms}$ (bare) / $53.1\text{ ms}$ (glove).
  - [x] Hardware-locked exact $15.000\text{ ms}$ SysTick frame synchronizer.
- [x] **Milestone 5: Online Dual-Channel Hardware Defect Diagnostics & ASIL-B Safety Engine** (`v1.0.5-m5`)
  - [x] Dual-window electrode Short-to-GND ($< 300\text{ LSB}$) and Short-to-VDD ($> 3800\text{ LSB}$) detection.
  - [x] Bridging resistor $R_{525}$ ($10\text{ k}\Omega$) open-circuit / degradation monitoring ($|v_{14} - v_{15}| > 250\text{ LSB}$).
  - [x] ADC reference voltage $V_{\text{REFH}}$ supply drift ($\pm 5\%$) monitoring via Bandgap Ch27 ($4750 \sim 5250\text{ mV}$).
  - [x] ASIL-B fail-safe clamp (unconditional touch suppression during hardware faults, Red LED indicator).
  - [x] Debounce qualification in 2 cycles ($\le 30\text{ ms}$) and auto-recovery self-healing in 8 cycles ($\le 120\text{ ms}$).
  - [x] Live hardware telemetry verification on S32K144W EVB with zero false failsafes (`Diag:OK`).
- [ ] **Milestone 6: VLPS Low-Power Sleep/Wakeup Integration & 12h Burn-in Test** (PLANNED)
  - [ ] LPIT periodic wakeup from PMC VLPS mode ($\le 60\text{ }\mu\text{s}$ active execution).
  - [ ] Deep-sleep power target $45 \sim 70\text{ }\mu\text{A}$.
  - [ ] 12-hour continuous burn-in stability test with zero false triggers.

---

## Project Structure

```
.
├── board/                         # Pin initialization configuration (PORT_CI)
├── generate/                      # RTD module configuration (Clock, OsIf, MEX)
├── include/                       # Public Application Headers
│   ├── touch_cvd.h                # M1: CVD physical layer acquisition & calibration
│   ├── touch_filter.h             # M2: LFSR, MFP burst, alpha-trim, 1D adaptive Kalman
│   ├── touch_baseline.h           # M3: 4-zone asymmetric dynamic baseline tracker
│   ├── touch_tinyml.h             # M4: Temporal envelope feature classifier
│   ├── touch_diag.h               # M5: On-line hardware defect diagnostics (ASIL-B)
│   └── uart_telemetry.h           # High-speed ASCII streaming telemetry
├── Project_Settings/              # Linker scripts, startup code, debugger config
├── RTD/                           # NXP Real Time Drivers 3.0.0 (AUTOSAR 4.7)
├── src/                           # Implementation Sources
│   ├── main.c                     # Engine orchestrator, SysTick sync & diagnostics
│   ├── touch_cvd.c                # CVD physical driver & 32x ADC calibration
│   ├── touch_filter.c             # Multi-frequency burst & adaptive Kalman
│   ├── touch_baseline.c           # 4-zone baseline state machine & recovery
│   ├── touch_tinyml.c             # Envelope feature extraction & hysteresis
│   ├── touch_diag.c               # Hardware defect diagnostics & auto-recovery
│   └── uart_telemetry.c           # Phased 115200 bps telemetry output
├── tests/                         # Automated Python Contract Verification Suites
│   ├── test_m2_contract.py        # M2: Filter chain, noise sigma, EFT immunity
│   ├── test_m3_contract.py        # M3: Touch-on-boot, slow approach, thermal drift
│   ├── test_m4_contract.py        # M4: Confusion matrix, water spray, glove detection
│   └── test_m5_contract.py        # M5: Fault injection, fail-safe interlock, recovery
├── tools/                         # Automated flashing and capture scripts
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools project file
├── README.md                      # English specification & progress documentation
└── README_zh.md                   # 繁體中文完整技術規格書與進度報告
```

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

# 4. Run automated milestone contract test suites
python ../tests/test_m2_contract.py
python ../tests/test_m3_contract.py
python ../tests/test_m4_contract.py
python ../tests/test_m5_contract.py
```

---

## License

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
