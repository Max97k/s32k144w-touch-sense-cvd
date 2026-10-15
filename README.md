# S32K144W Capacitive Touch Sensing Reference Implementation (CVD & Rule-Based Classifier)

[![Target: NXP S32K144W](https://img.shields.io/badge/Target-NXP%20S32K144W-005a9c.svg)](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/s32k-automotive-mcus/s32k1-microcontrollers-for-general-purpose:S32K1)
[![Core: Arm Cortex-M4F](https://img.shields.io/badge/Core-Arm%C2%AE%20Cortex%C2%AE--M4F%20%40%2048MHz-blue.svg)](https://www.arm.com/products/silicon-ip-cpu/cortex-m/cortex-m4)
[![RTD: 3.0.0 (AUTOSAR 4.7)](https://img.shields.io/badge/RTD-3.0.0%20(AUTOSAR%C2%AE%204.7)-orange.svg)](https://www.nxp.com)
[![Toolchain: GCC 10.2](https://img.shields.io/badge/Toolchain-GCC%2010.2%20(0%20errors%20%7C%200%20warns)-brightgreen.svg)](https://gcc.gnu.org)
[![Design Style: Defensive C / Zero-Heap](https://img.shields.io/badge/Design-Defensive%20C%20%2F%20Zero--Heap-blue.svg)](include/touch_diag.h)
[![Diagnostics: Plausibility & Fault Checks](https://img.shields.io/badge/Diagnostics-Plausibility%20%26%20Fault%20Checks-brightgreen.svg)](include/touch_diag.h)
[![EMC: IEC 61000-4-6 / CISPR 25](https://img.shields.io/badge/EMC-Spread--Spectrum%20MFP-purple.svg)](https://www.iec.ch)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

English | [繁體中文](README_zh.md)

---

An embedded capacitive touch sensing firmware reference implementation designed for the **NXP Semiconductors® S32K144W evaluation board (`XS32K14WEVB-Q064`)**.

The firmware utilizes a dual-pin differential Capacitive Voltage Divider (CVD) charge-transfer topology on the onboard `SW7` copper pad (`PTC17` driving, bridging resistor `R525` $10\text{ k}\Omega$/$4.7\text{ k}\Omega$, `PTC16/ADC0_SE14` sensing, and external reference capacitor `C533` $10\text{ pF}$). It is built with **NXP Real Time Drivers (RTD 3.0.0)** and defensive embedded C practices (zero dynamic heap allocation, bounded polling loops, and continuous hardware plausibility diagnostics).

---

## Key Features

* **Bipolar Four-Phase Differential CVD**: Alternates positive and negative precharge & charge-sharing phases, yielding $\sim 2\times$ differential physical signal gain over single-ended sensing while rejecting common-mode supply noise.
* **Multi-Frequency Spread-Spectrum Burst Acquisition (MFP)**: 16-bit Galois LFSR (`0xB400`) pseudo-random sample delay hopping across coprime delay counts $\{15, 23, 31\}$ cycles to avoid EMI harmonic synchronization.
* **Cascaded Non-Linear Filtering Engine**: 9-sample burst acquisition with non-linear Alpha-trimmed mean (rejecting highest/lowest 2 extreme outlier spikes) cascaded into a 1D Adaptive Scalar Kalman Filter ($Q=0.35, R=1.0, \alpha=0.75$) with high-slew fast-path bypass.
* **4-Zone Asymmetric Dynamic Baseline Tracking**: Slew-rate bounded ambient drift tracking ($+1.5\text{ LSB/s}$ / $-2.0\text{ LSB/s}$), approach freeze zone ($0.4\times\text{TH}$), 15-second continuous contact timeout protection, and fast negative anomaly recovery ($\le 285\text{ ms}$) for touch-on-boot scenarios.
* **Rule-Based Temporal Envelope Feature Classifier**: Lightweight heuristic decision tree with soft-margin scoring extracting 4 time-domain envelope features (attack slope, dynamic SNR, normalized curvature variance, sustain ratio) combined with 3-step confidence sliding-window hysteresis (0.64 trigger / 0.38 release) and dropout hold.
* **Hardware Plausibility Diagnostics & Fail-Safe Interlock**: Continuous on-line monitoring of electrode Short-to-GND ($< 300\text{ LSB}$), Short-to-VDD ($> 3800\text{ LSB}$), $R_{525}$ bridging integrity ($< 250\text{ LSB}$ balance delta), and ADC reference $V_{\text{REFH}}$ drift ($\pm 5\%$) via internal Bandgap Channel 27, automatically disarming touch output during fault conditions.
* **Hardware-Locked 15.0 ms Frame Synchronizer**: Driven by ARM Cortex-M SysTick ($48\text{ MHz}$) for zero-jitter 66.7 Hz frame timing. Measured CPU execution time is $\sim 195\text{--}215\text{ }\mu\text{s}$ per cycle ($\sim 1.4\%$ CPU load).
* **Defensive Embedded C Architecture**: 0 Bytes heap allocation (static memory only), timeout guards on all hardware polling loops, and static stack consumption $< 250\text{ Bytes}$.

---

## Hardware Pinout (XS32K14WEVB-Q064)

| Signal / Role | MCU Port | 64-LQFP Pin | External Components | Description |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (Drive)** | `PTC17` | Pin 27 | $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$), $R_{172}$ ($0\Omega$), `SW7` | CVD charge transfer drive / `ADC0_SE15` |
| **TOUCH_B (Sense)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ to GND), $R_{525}$ | CVD charge transfer sense / `ADC0_SE14` |
| **Touch Pad** | `SW7` | Onboard PCB | $15\text{ mm} \times 15\text{ mm}$ Copper electrode | Sensing capacitance $C_x$ |
| **UART_TX** | `PTC9` | Pin 35 | OpenSDA Virtual COM TX (ALT2) | 115200 bps 8N1 Telemetry stream |
| **UART_RX** | `PTC8` | Pin 36 | OpenSDA Virtual COM RX (ALT2) | Serial receive channel |
| **RED LED** | `PTE7` | Pin 39 | GPIO Output | Standby indicator (ON when idle/released, OFF when touched; fault indicator) |
| **GREEN LED** | `PTE0` | Pin 60 | GPIO Output | Touch confirmed indicator (ON when touch is active) |
| **BLUE LED** | `PTE3` | Pin 13 | GPIO Output | 1.0s Heartbeat pulse (0.1s ON / 0.9s OFF, synchronized with dashboard) |

> **Note on UART Pins**: On the XS32K14WEVB-Q064 board, OpenSDA Virtual COM is wired to `PTC8` (RX) and `PTC9` (TX). `PTC6` and `PTC7` route to the J5 header expansion and are kept un-muxed to prevent internal input contention on `LPUART1_RX`.

---

## Architecture & Firmware Pipeline

```
                  ┌───────────────────────────────────────────────┐
                  │   SW7 Touch Electrode (PTC17 / PTC16 CVD)     │
                  └───────────────────────┬───────────────────────┘
                                          │ 18-sample SAR conversions (4-phase CVD)
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │  Phase 1: Multi-Frequency Burst (MFP + LFSR)  │
                  └───────────────────────┬───────────────────────┘
                                          │ Raw burst samples
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ Phase 2: Alpha-Trimmed Mean & Outlier Filter  │
                  └───────────────────────┬───────────────────────┘
                                          │ Outlier-free measurement z_meas, spread
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ Phase 3: 1D Adaptive Scalar Kalman Filter     │
                  └───────────────────────┬───────────────────────┘
                                          │ Filtered state estimate x_est
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ Phase 4: 4-Zone Asymmetric Dynamic Baseline   │
                  └───────────────────────┬───────────────────────┘
                                          │ Dynamic signal shift Δdyn = x_est - baseline
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ Phase 5: Rule-Based Temporal Envelope Engine  │
                  │ (Attack slope, Dyn SNR, Curvature, Sustain)   │
                  └───────────────────────┬───────────────────────┘
                                          │ Candidate touch decision
                                          ▼
┌───────────────────────────────┐         │
│ Phase 6: Hardware Defect      │         │
│ Diagnostics & VREFH Monitor   ├─────────┴───────────────────────┐
└───────────────┬───────────────┘                                 │
                │ Active fault status                             ▼
                ▼                                   ┌───────────────────────────┐
  ┌───────────────────────────┐                     │ Final Qualified Touch Out │
  │ Fail-Safe Safety Clamp    ├────────────────────►│ (LED, Telemetry, Scope)   │
  │ (Forces touch to 0 on err)│                     └───────────────────────────┘
  └───────────────────────────┘
```

---

## Real-Time Telemetry Protocol

The board streams real-time ASCII CSV telemetry at 66.7 Hz (every 15.0 ms) over the OpenSDA Virtual COM port (`115200` bps, `8N1`).

### CSV Stream Format:
```
D,<Cycle>,<A>,<B>,<DeltaD>,<Xest>,<Base>,<DynDelta>,<Class>,<Conf>,<Touch>,<ExecUs>,<Heartbeat>,<Status>
```

| Field | Type | Description |
| :--- | :---: | :--- |
| `Cycle` | uint32 | Monotonic cycle counter incremented every 15.0 ms frame |
| `A` | uint16 | Raw ADC reading on electrode PTC17 (`ADC0_SE15`) |
| `B` | uint16 | Raw ADC reading on reference PTC16 (`ADC0_SE14`) |
| `DeltaD` | int16 | Differential CVD synthesis: $V_B - V_A$ |
| `Xest` | float | 1D Adaptive Scalar Kalman Filter estimated state |
| `Base` | float | 4-Zone asymmetric dynamic baseline tracking level |
| `DynDelta` | float | Baseline-compensated signal displacement: $x_{\text{est}} - \text{baseline}$ |
| `Class` | uint32 | Category code: `0` Idle, `1` Bare Finger, `2` Glove/Acrylic, `3` Water, `4` Edge Swipe |
| `Conf` | float | 3-step confidence score ($0.00 \sim 1.00$) |
| `Touch` | uint32 | Final verified touch state (`1` Pressed, `0` Released) |
| `ExecUs` | uint32 | Core algorithm execution time in microseconds (SysTick measured, typically $\sim 210\text{ }\mu\text{s}$) |
| `Heartbeat`| uint32 | Heartbeat LED status (`1` Active/ON 0.1s, `0` Idle/OFF 0.9s) |
| `Status` | string | ASCII status string: `TOUCH` or `IDLE` |

---

## Project Structure

```
.
├── board/                         # Pin initialization configuration (PORT_CI)
├── data/                          # Telemetry and validation datasets
│   ├── hardware/                  # Real target UART telemetry captures (.csv)
│   └── simulated/                 # Contract verification synthetic datasets (.csv)
├── freemaster/                    # FreeMASTER Lite 1.5.1 oscilloscope dashboard
├── generate/                      # RTD module configuration (Clock, OsIf, MEX)
├── include/                       # Public Application Headers
│   ├── touch_cvd.h                # CVD physical layer acquisition & calibration
│   ├── touch_filter.h             # LFSR, MFP burst, alpha-trim, 1D adaptive Kalman
│   ├── touch_baseline.h           # 4-zone asymmetric dynamic baseline tracker
│   ├── touch_tinyml.h             # Temporal envelope feature classifier
│   ├── touch_diag.h               # Hardware defect diagnostics & fail-safe engine
│   ├── touch_sense.h              # Unified touch sensing engine
│   └── uart_telemetry.h           # High-speed ASCII streaming telemetry
├── Project_Settings/              # Linker scripts, startup code, debugger config
├── RTD/                           # NXP Real Time Drivers 3.0.0
├── src/                           # Implementation Sources
│   ├── main.c                     # Engine orchestrator, SysTick sync & diagnostics
│   ├── touch_cvd.c                # CVD physical driver & 32x ADC calibration
│   ├── touch_filter.c             # Multi-frequency burst & adaptive Kalman
│   ├── touch_baseline.c           # 4-zone baseline state machine & recovery
│   ├── touch_tinyml.c             # Envelope feature extraction & hysteresis
│   ├── touch_diag.c               # Hardware defect diagnostics & auto-recovery
│   ├── touch_sense.c              # Unified touch sense driver
│   └── uart_telemetry.c           # LPUART1 115200 bps telemetry output
├── tests/                         # Automated Python Contract Verification Suites
│   ├── test_m1_contract.py        # M1: Untouched baseline distribution
│   ├── test_m2_contract.py        # M2: Filter chain, noise sigma, EFT immunity
│   ├── test_m3_contract.py        # M3: Touch-on-boot, slow approach, thermal drift
│   ├── test_m4_contract.py        # M4: Confusion matrix, water spray, glove detection
│   └── test_m5_contract.py        # M5: Fault injection, fail-safe interlock, recovery
├── tools/                         # Automated flashing, live recording and analysis tools
│   ├── analyze_press.py           # Hardware cued press validation report
│   ├── capture_telemetry.py       # 100-sample raw dataset capture tool
│   ├── flash_target.py            # OpenSDA PEMicro automated flash programmer
│   └── record_live.py             # Live UART streaming telemetry recorder
├── run_freemaster.bat             # One-click FreeMASTER Lite dashboard launcher
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools project file
├── README.md                      # English specification & documentation
└── README_zh.md                   # 繁體中文技術規格與說明文件
```

---

## Build and Flashing Instructions

### Prerequisites
* **S32 Design Studio for S32 Platform** (Version 3.6.11) with GCC 10.2 Bare-Metal toolchain and NXP S32K1xx RTD 3.0.0.
* Python 3.8+ with `pyserial` (for flashing and telemetry tools).

### Building with Make:
```powershell
# 1. Setup S32DS toolchain environment
$env:PATH = "C:\NXP\S32DS.3.6.11\S32DS\build_tools\msys32\usr\bin;C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin;" + $env:PATH

# 2. Compile via GNU Make (0 errors, 0 warnings)
cd Debug_FLASH
make -j8 all
```

### Flashing to Target Board:
```powershell
# Flash using OpenSDA PEMicro
python tools/flash_target.py
```

### Running Automated Test Suites:
```powershell
python -B tests/test_m1_contract.py
python -B tests/test_m2_contract.py
python -B tests/test_m3_contract.py
python -B tests/test_m4_contract.py
python -B tests/test_m5_contract.py
```

---

## FreeMASTER Lite 1.5.1 Live Oscilloscope Dashboard

The project includes an HTML5 oscilloscope dashboard running on **NXP FreeMASTER Lite 1.5.1**, providing real-time visual inspection of raw physical CVD differences, Kalman filter responses, dynamic baseline tracking, and confidence metrics.

```
 [ S32K144W Hardware EVB ]
            │ (PTC9 UART TX @ 115200 bps, 66.7 Hz)
            ▼
 [ telemetry_bridge.py ] ───── (WebSocket ws://localhost:8090) ─────► [ FreeMASTER Lite 1.5.1 ]
                                                                             │         │
                                     ┌───────────────────────────────────────┘         │
                                     ▼                                                 ▼
                        [ touch_scope.html Dashboard ]                    [ NXP FreeMASTER MCP ]
                        - 4-Channel Live Rolling Scope                    - ws.connect()
                        - Glowing Touch Status Pill                       - rpc.StartComm()
                        - Envelope Confidence Gauge                       - rpc.ReadVariable()
                        - Execution Time Monitor (~210 µs)                - rpc.WriteVariable()
```

### Launching the Dashboard:
Double-click `run_freemaster.bat` or run:
```cmd
run_freemaster.bat
```
Then open your web browser to:
```
http://localhost:8090/touch_scope.html
```

---

## References & Technical Standards

This implementation references and builds upon the following academic papers and semiconductor application notes:

1. **[1]** S. M. Hong, J.-W. Park, J.-H. Jeong, and S. Y. Kim, "Real-Time Defect Detection of Capacitive Touch Pads for Hands-Off Detection in Advanced Driver Assistance Systems," *Applied Sciences*, vol. 16, no. 4, art. no. 1675, Feb. 2026.
2. **[2]** X. Feng et al., "A Kalman Filter-Assisted Data-Predictive SAR ADC With Reduced Switching Energy for Low-Power Applications," *arXiv preprint arXiv:2607.16139*, Jul. 2026.
3. **[3]** X. Ouyang, H. Kuang, D. Kong, Z. Cheng, and H. Yuan, "A Phase-Adjustable Noise-Shaping SAR ADC for Mitigating Parasitic Capacitance Effects from PIP Capacitors," *Sensors*, vol. 25, no. 19, art. no. 6029, Oct. 2025.
4. **[4]** Y.-H. Hwang, "Study of Circuit Techniques for Enhancing Display Noise Immunity in Analog Front-end of Mutual-capacitive Touch Systems," *JSTS*, vol. 24, no. 5, pp. 473–482, Oct. 2024.
5. **[5]** S. Seong, S. Lee, S. Bae, and M. Lee, "Self Capacitance Mismatch Calibration Technique for Fully-Differential Touch Screen Panel Self Capacitance Sensing System," *Sensors*, vol. 23, no. 7, art. no. 3779, Apr. 2023.
6. **[6]** H. Pyeon, H. Kim, R. C. Kim, G. Oh, and S. Lim, "Deep Learning-Based Driver's Hands on/off Prediction System Using In-Vehicle Data," *Sensors*, vol. 23, no. 3, art. no. 1442, Jan. 2023.
7. **[7]** H. Nam, K.-H. Seol, J. Lee, H. Cho, and S. W. Jung, "Review of Capacitive Touchscreen Technologies: Overview, Research Trends, and Machine Learning Approaches," *Sensors*, vol. 21, no. 14, art. no. 4776, Jul. 2021.
8. **[8]** Y.-H. Yu and T.-Y. Sun, "A Pseudo-Differential Measuring Approach for Implementing Microcontroller-Based Capacitive Touch Sensing in Low-Power Quality Situation," *IEEE Sensors Journal*, vol. 16, no. 2, pp. 390–399, Jan. 2016.
9. **[9]** S. Sun, "GPIS Touch Sense Solution," NXP Semiconductors, Automotive CAS Technical Presentation, Doc. No. APF-AUT-T3280, Sep. 2018.
10. **[10]** B. Davison, "mTouch™ Sensing Solution Acquisition Methods Capacitive Voltage Divider," Microchip Technology Inc., Application Note AN1478, 2012.
11. **[11]** Microchip Technology Inc., "Touch Sensing Using Analog-to-Digital Converter With Computation (ADCC)," Microchip Developer Help, 2026.
12. **[12]** Texas Instruments Inc., "Enabling Noise Tolerant Capacitive Touch HMIs With MSP CapTIvate™ Microcontrollers," Application Report SLAY045B, 2017.
13. **[13]** NXP Semiconductors, "S32K1xx ADC Guidelines, Spec, and Configuration," Application Note AN12217, Rev. 0, Jun. 2018.
14. **[14]** NXP Semiconductors, "S32K144EVB-Q100 Evaluation Board Schematic," Doc. No. SCH-29248 / SPF-29248, Rev. C, Mar. 2018; and "S32K1xx Data Sheet," Rev. 14, 2021.

---

## License

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
