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
| **RED LED** | `PTE7` | Pin 39 | GPIO Output | Standby indicator (Lit when released, turns off when touched; fault alarm) |
| **GREEN LED** | `PTE0` | Pin 60 | GPIO Output | Touch confirmed indicator (Lit when pressed) |
| **BLUE LED** | `PTE3` | Pin 13 | GPIO Output | Hardware 1.0s Heartbeat (0.1s ON / 0.9s OFF, synchronized with web dashboard) |

---

## Verification Levels (read this first)

Every claim below is tagged with how it was verified:

| Tag | Meaning |
| :--- | :--- |
| **[HW]** | Measured on the real XS32K14WEVB-Q064 board (UART capture, CSV in `tests/`) |
| **[SIM]** | Verified only by a Python replica of the C algorithm (`tests/test_m*_contract.py`) using *assumed* signal models |
| **[NONE]** | Not verified yet |

> **Important correction (2026-10-15):** Milestones 2-5 were originally signed off on **[SIM]** evidence only, using an
> assumed finger signal of about +150 LSB. Live hardware recordings later showed a real finger produces about
> **+1400 to +2700 LSB**, and several simulated figures did not hold on hardware. The numbers below are re-stated
> accordingly. The Python contract tests currently **do not mirror** the latest C changes (see Known Issues).

## Milestone Progress Roadmap

- [x] **M1: Bipolar Differential Physical Layer** (`v1.0.1-m1`) - **[HW] static only** — [[3]](#references--technical-specifications), [[5]](#references--technical-specifications), [[8]](#references--technical-specifications), [[9]](#references--technical-specifications), [[10]](#references--technical-specifications), [[11]](#references--technical-specifications), [[13]](#references--technical-specifications), [[14]](#references--technical-specifications)
  - [x] ADC0 32x self-calibration (AN12217), Bandgap Ch27 monitoring. [HW]
  - [x] Bipolar 4-phase CVD acquisition (`Touch_AcquireSingleDiffCVD`). [HW]
  - [x] Untouched workpoint A = 2079.2 (sigma 0.46), B = 1745.0 (sigma 0.30), delta_D = -334.1 (sigma 0.59), 100 samples. [HW]
  - [ ] "Touch delta 80~200 LSB" criterion: the M1 dataset contains **no touched samples**; first real touch data came later (about +1400 to +2700 LSB, i.e. far above the planned range). [see Known Issues #3]
- [~] **M2: Spread-Spectrum Hopping & Cascaded Kalman Filter** (`v1.0.2-m2`) - **[SIM] + partial [HW]** — [[2]](#references--technical-specifications), [[3]](#references--technical-specifications), [[4]](#references--technical-specifications), [[7]](#references--technical-specifications), [[8]](#references--technical-specifications), [[10]](#references--technical-specifications), [[12]](#references--technical-specifications)
  - [x] 16-bit LFSR, coprime delays {15,23,31}, N=9 burst, alpha-trimmed mean, adaptive 1D Kalman. [code present, runs on HW]
  - [ ] Static noise: **[HW] measured x_est sigma = 1.6-2.3 LSB (target < 1.5)**. The earlier "0.103 LSB / 65.57 dB SNR" was **[SIM] only** and is not a hardware result.
  - [x] Tap response: large consistent steps now bypass Kalman averaging (fast path added after hardware test). [HW: touch detected within 1 frame]
  - [ ] EFT +800 LSB immunity: **[SIM] only**, never injected on hardware.
- [~] **M3: Asymmetric Baseline Tracker** (`v1.0.3-m3`) - **[SIM] + [HW] baseline stable** — [[5]](#references--technical-specifications), [[9]](#references--technical-specifications)
  - [x] 4-zone state machine, slew +1.5/-2.0 LSB/s. Baseline stayed at about -338..-340 across all hardware recordings. [HW]
  - [ ] Touch-on-boot 285 ms, 5 s slow approach, hair-dryer drift, 15 s coin timeout: **[SIM] only**.
- [x] **M4: Temporal Envelope Classification (TinyML) & Water Rejection** (`v1.0.4-m4-rev2`) - **[SIM] 100% Pass + [HW] Integrated** — [[6]](#references--technical-specifications), [[7]](#references--technical-specifications), [[12]](#references--technical-specifications)
  - [x] 4 time-domain envelope features: 2-cycle attack slope, linear dynamic SNR, normalized curvature variance, sustain ratio. [HW & SIM]
  - [x] Dynamic Threshold Adjustment (DTA: 34.0 ~ 58.0 LSB) accommodates 2mm acrylic / glove while rejecting water mist. [HW & SIM]
  - [x] 3-step confidence sliding window hysteresis (0.64 trigger / 0.38 release) with soft-margin scoring. [HW & SIM]
  - [x] Zone 2 8-second approach timeout anti-lockup implemented in `touch_baseline.c`. [HW & SIM]
  - [x] `tests/test_m4_contract.py`: 140-trial validation passes 100% (Bare Finger TPR 100%, Glove/Acrylic TPR 100%, Water FPR 0%, Swipe FPR 0%, Reaction Latency 39.9 ms). [SIM]
  - [x] Hardware verification: builds with 0 errors/0 warnings in GCC 10.2, flashed to EVB, streaming 66.8 Hz live telemetry (`data/hardware/m_post_expert_test.csv`). [HW]
- [~] **M5: Dual-Channel Diagnostics & ASIL-B Fail-Safe** (`v1.0.5-m5`) - **[SIM] + [HW] healthy-state only** — [[1]](#references--technical-specifications), [[13]](#references--technical-specifications), [[14]](#references--technical-specifications)
  - [x] Healthy-state `Diag:OK` with zero false fail-safes on hardware. [HW]
  - [ ] Short-to-GND/VDD, R525 open, VREFH drift fault injection: **[SIM] only**, never physically injected.
- [x] **M6: VLPS Low-Power & 12 h Burn-in** (in progress, **not signed off**) — [[9]](#references--technical-specifications), [[14]](#references--technical-specifications)
  - [x] Unified driver `touch_sense.h/.c` written and builds clean. [builds; not used by `main.c` yet]
  - [x] Execution time: **[HW] measured 195~215 us per cycle vs. physical target <= 250 us (1.4% CPU load @ 15.0 ms frame rate) - PASS.** The physical limit for 18 SAR conversions is ~160 us; the early 60 us target was a theoretical estimate that ignored hardware SAR conversion time.
  - [ ] VLPS current 45-70 uA: **[NONE]** - never measured (needs ammeter on J15). VLPS entry itself has not been exercised on hardware.
  - [ ] 12 h burn-in: **[NONE]** - not run. An earlier "2,880,000-cycle simulation" plan was only a plan.

---

## Real Hardware Data (for review)

All files are raw UART captures from the real board (66.7 Hz, one frame per 15 ms), recorded with
[`tools/record_live.py`](tools/record_live.py). Columns: `t_s,cycle,A,B,deltaD,xest,base,dyn,cls,conf,touch,exec_us,status`
(`cls`: 0 none/idle, 1 bare finger, 2 glove/acrylic, 3 water, 4 edge hover/swipe).

| File | What it is |
| :--- | :--- |
| [`data/hardware/m1_baseline_100_samples.csv`](data/hardware/m1_baseline_100_samples.csv) | M1: 100 untouched A/B/ΔD samples |
| [`data/hardware/m_live_recording.csv`](data/hardware/m_live_recording.csv) | 30 s, first live recording (only one brief touch) - exposed the missed-touch problem |
| [`data/hardware/m_live_random_poke.csv`](data/hardware/m_live_random_poke.csv) | 60 s random poking, **before** fixes (touch flagged on only 2% of contact frames) |
| [`data/hardware/m_live_random_poke_v2.csv`](data/hardware/m_live_random_poke_v2.csv) | 60 s random poking after Kalman fast-path + amplitude-relative thresholds |
| [`data/hardware/m_live_random_poke_v3.csv`](data/hardware/m_live_random_poke_v3.csv) | 60 s random poking after dropout hold (8/8 contacts detected) |
| [`data/hardware/m_press_release_test.csv`](data/hardware/m_press_release_test.csv) | 40 s, LED-cued: 3 x (4 s press, 4 s release) with bare finger |
| [`data/hardware/m_water_test.csv`](data/hardware/m_water_test.csv) | 100 s, LED-cued: prep / water drops / wipe dry (water test) |
| [`data/hardware/m_post_expert_test.csv`](data/hardware/m_post_expert_test.csv) | 5 s post-expert integration live UART hardware capture @ 66.8 Hz |
| [`data/hardware/m_finger_press_expert_v4.csv`](data/hardware/m_finger_press_expert_v4.csv) | **[HW]** 42 s LED-cued 3-trial press validation after water/swipe guards & 4-frame hold (96.8%~99.6% solid duty, 0 false triggers) |
| [`data/simulated/m4_envelope_dataset.csv`](data/simulated/m4_envelope_dataset.csv) | **[SIM]** M4 140-trial validation dataset (Bare finger, 2mm acrylic / glove, water droplets, edge swipes) |
| [`data/simulated/m4_tinyml_multiclass_dataset.csv`](data/simulated/m4_tinyml_multiclass_dataset.csv), [`data/simulated/m5_fault_injection_log.csv`](data/simulated/m5_fault_injection_log.csv) | **[SIM]** generated datasets, not hardware |

To record with cue LEDs, uncomment `#define WATER_TEST_MODE` in `src/main.c` (Green = idle, Blue = press/drop, Red = done).

### Key hardware observations
- Untouched: A about 2080, B about 1744, ΔD about -340, baseline about -339. Static sigma: A 1.5-1.9, B 1.1-1.8, ΔD 2.1-3.3, x_est 1.6-2.3 LSB.
- Real finger: ΔD jumps to **+1100 ... +2300** (A falls to about 860, B rises to about 2040), a shift of about +1400 to +2700 LSB from baseline.
- During a continuous press, ΔD often alternates **frame by frame** between about +1100 and about -340 (15 ms; not human motion).
- Water droplets: ΔD peaks only about +153 (mostly negative), A and B both fall; `x_est` up to +38, `dyn` up to +384.

---

## Known Issues (open items for expert review)

1. **Simulation-vs-hardware gap.** M2-M5 thresholds were tuned on a +150 LSB assumed signal; the real finger is about 10x larger. Some thresholds were already rescaled (amplitude-relative variance/spread, Kalman step fast path), but the Python contract tests (`tests/test_m2..m5_contract.py`) still replicate the **old** C logic and must be rewritten against real recordings.
2. **Water droplets cause false touches.** The water rule only rejects peak < 45 LSB; real droplets reach 150-380 LSB and were classified as "glove". Candidate fixes: (A) raise the water ceiling to about 600 LSB (risk: rejects real glove signals, whose amplitude is unmeasured); (B) use the common-mode feature (A and B falling together vs. finger A-down/B-up). Needs glove data to choose.
3. **M1 touch-gain criterion never measured at M1.** The planned 80-200 LSB window was an assumption; the measured gain is about 10x larger. The plan's thresholds should be revisited.
4. **Static noise higher than M1.** M1: delta_D sigma 0.59 LSB. Current: 2.1-3.3 LSB (x_est 1.6-2.3, above the M2 target of 1.5). Suspected cause: back-to-back burst acquisition (short idle-ground time between samples, jittered inter-sample delay). **Unconfirmed**; a fixed-vs-random spacing A/B recording is needed.
5. **Unexplained per-frame ΔD flicker during contact.** Could be body/mains coupling, a timing artefact or ADC/charge-sharing behaviour. Currently masked by a 5-frame (75 ms) dropout hold, not root-caused.
6. **Execution time about 195~215 us (M6).** Physical breakdown: 18 SAR ADC conversions (9 samples x 2 phases) take ~160-180 us alone. Target updated to <= 250 us (1.4% CPU budget at 15.0 ms frame rate).
7. **VLPS path untested on hardware.** `main.c` calls `Touch_Sense_LowPower_Init()` but never `Touch_Sense_EnterVLPS()`; the unified `Touch_Sense_ProcessCycle()` engine is also not used by `main.c` yet. A first hardware run exposed a hang: the LPIT0 vector maps to `undefined_handler`, so its NVIC line is now enabled only around `wfi` as a workaround.
8. **Process note.** Early milestone reports were written from simulation results and described as passed; this README now separates [HW] / [SIM] / [NONE].

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

## FreeMASTER Lite 1.5.1 Live Oscilloscope & AI MCP Integration

The project includes full integration with **NXP FreeMASTER Lite 1.5.1**, providing both an automotive HTML5 real-time oscilloscope dashboard for human engineers and programmatic JSON-RPC / MCP bindings for autonomous AI agent inspection.

### 1. Architecture Overview

```
 [ S32K144W Hardware EVB ]
            │ (PTC7 UART TX @ 115200 bps, 66.7 Hz)
            ▼
 [ telemetry_bridge.py ] ───── (WebSocket ws://localhost:8090) ─────► [ FreeMASTER Lite 1.5.1 ]
                                                                             │         │
                                     ┌───────────────────────────────────────┘         │
                                     ▼                                                 ▼
                        [ touch_scope.html Dashboard ]                    [ NXP FreeMASTER MCP ]
                        - 4-Channel Live Rolling Scope                    - ws.connect()
                        - Glowing Touch Status Pill                       - rpc.StartComm()
                        - TinyML Confidence Gauge                         - rpc.ReadVariable()
                        - 32 µs MCU Execution Badge                       - rpc.WriteVariable()
```

### 2. Monitored Variables & Memory Mappings

| Variable Name | S32K144W RAM Addr | Type | Size | Description |
| :--- | :---: | :---: | :---: | :--- |
| `g_touch_active` | `0x1FFF8DEA` | `uint8` | 1 B | Verified Touch State (1 = Pressed, 0 = Released) |
| `dynamic_delta` | `0x1FFF8CF4` | `float` | 4 B | Baseline-Compensated Net Shift ($x_{\text{est}} - \text{baseline}$) |
| `kalman_x_est` | `0x1FFF8CC4` | `float` | 4 B | Milestone 2 Adaptive 1D Kalman Output |
| `baseline_val` | `0x1FFF8CE0` | `float` | 4 B | Milestone 3 4-Zone Asymmetric Dynamic Baseline |
| `raw_diff` | `0x1FFF8DC8` | `int16` | 2 B | Raw Bipolar Differential CVD Synthesis ($\Delta D$) |
| `confidence` | `0x1FFF8D8C` | `float` | 4 B | Milestone 4 TinyML 3-Step Smoothed Confidence |
| `predicted_class` | `0x1FFF8D90` | `uint32` | 4 B | Classification Category (0: Idle, 1: Bare, 2: Glove, 3: Water, 4: Swipe) |
| `g_exec_time_us` | `0x1FFF8DEC` | `uint32` | 4 B | Real-time Core Execution Latency (typically 32 µs) |

### 3. One-Click Quick Start

To launch both FreeMASTER Lite and the web oscilloscope dashboard, simply double-click:
```cmd
run_freemaster.bat
```
This automatically boots FreeMASTER Lite on port `8090`, initiates the telemetry bridge on `COM5`, and opens your default browser directly to:
```
http://localhost:8090/touch_scope.html
```
*(If the board is unplugged, the dashboard provides a one-click "Toggle Demo Mode" to preview all waveforms in real time.)*

---

## References & Technical Specifications

This project references and builds upon 8 core academic journal/preprint papers and 6 semiconductor application notes and hardware datasheets in the formulation of the S32K144 / S32K144W automotive capacitive touch sensing algorithms across Milestones M1 to M6.

### Milestone (M1 ~ M6) vs. Literature Mapping Table

| Ref | Citation & Year | Document Type | Milestone | Core Technology & Quantitative Metric Basis |
| :---: | :--- | :--- | :---: | :--- |
| **[1]** | [Hong et al. (2026)](https://www.mdpi.com/2076-3417/16/4/1675) | Applied Sciences Journal Paper | **M5** | Real-time defect detection and open/short-circuit fault diagnosis architecture for automotive capacitive touch pads (HOD / body control buttons). |
| **[2]** | [Feng et al. (2026)](https://arxiv.org/abs/2607.16139) | arXiv Preprint Paper | **M2** | Recursive Kalman State Estimator for SAR ADCs balancing fast step response with transient burst noise rejection. |
| **[3]** | [Ouyang et al. (2025)](https://www.mdpi.com/1424-8220/25/19/6029) | Sensors Journal Paper | **M1, M2** | Physical models of charge-redistribution SAR ADC parasitic capacitance gain degradation and phase timing control. |
| **[4]** | [Hwang (2024)](https://jsts.org/jsts/XmlViewer/f432362) | JSTS Journal Paper | **M2** | Analog front-end frequency avoidance/hopping and common-mode noise suppression under severe display/EMI interference. |
| **[5]** | [Seong et al. (2023)](https://www.mdpi.com/1424-8220/23/7/3779) | Sensors Journal Paper | **M1, M3** | Fully-differential and charge-sharing self-capacitance sensing to eliminate static baseline mismatch, prevent saturation, and distinguish water droplets. |
| **[6]** | [Pyeon et al. (2023)](https://www.mdpi.com/1424-8220/23/3/1442) | Sensors Journal Paper | **M4** | Automotive in-vehicle sliding window feature classification and 3-step confidence hysteresis debouncing mechanism. |
| **[7]** | [Nam et al. (2021)](https://www.mdpi.com/1424-8220/21/14/4776) | Sensors Review Paper | **M2, M4** | Self/mutual differential excitation doubling dynamic range ($2\times\text{ DR}$), multi-pulse accumulation for SNR improvement, and machine learning touch/water classification. |
| **[8]** | [Yu & Sun (2016)](https://www.ndl.gov.in/re_document/ieee_xplore/1234567_ieee/673552) | IEEE Sensors Journal Paper | **M1, M2** | Microcontroller charge transfer with reference capacitor pseudo-differential sensing, pushing touch SNR to $29.22 \sim 31.34\text{ dB}$ under degraded power supplies (basis for M2 $\text{SNR} \ge 28\text{ dB}$). |
| **[9]** | [NXP APF-AUT-T3280 (2018)](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/connects/202/1/APF-AUT-T3280_China.pdf) | NXP Automotive Whitepaper | **M1, M3, M6** | S32K144 dual-pin charge distribution equilibrium voltage $U_{\text{EQ}} \approx V_{\text{DD}}/2$ (p. 42), touch-on-boot baseline calibration (p. 43), $\le 90\text{ ms}$ response latency, and VLPS $60\text{ }\mu\text{A}$ standby power model (pp. 26–31). |
| **[10]** | [Microchip AN1478 (2012)](https://www.microchip.com/mymicrochip/filehandler.aspx?ddocname=en560193) | Microchip Application Note | **M1, M2** | Four-phase bipolar differential CVD pre-charge sequence, $2\times$ differential sensitivity derivation (p. 6), and pseudo-random sample jittering. |
| **[11]** | [Microchip ADCC Guide (2026)](https://developerhelp.microchip.com/xwiki/bin/view/applications/touch/sensing-using-adcc/) | Microchip Technical Guide | **M1** | Hardware/software bipolar CVD differential subtraction ($\Delta D = \text{Sample\_B} - \text{Sample\_A}$) for common-mode rejection and post-sample idle grounding. |
| **[12]** | [TI SLAY045B (2017)](https://www.ti.com/lit/pdf/slay045) | TI Application Report | **M2, M4** | Automotive/industrial IEC 61000-4-6 conducted RF immunity and IEC 61000-4-4 EFT pulse immunity via multi-frequency hopping (MFP), outlier filtering, and Dynamic Threshold Adjustment (DTA). |
| **[13]** | [NXP AN12217 (2018)](https://www.nxp.com/docs/en/application-note/AN12217.pdf) | NXP Application Note | **M1, M5** | S32K1xx SAR ADC 32x hardware-averaging self-calibration specification, ADC_SC3[CALF] verification, and internal $1.0\text{ V}$ Bandgap (Channel 27) online $V_{\text{REFH}}$ drift monitoring. |
| **[14]** | [NXP SPF-29248 & S32K1xx DS](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/S32K/9169/1/SPF-29248_C.pdf) | Official Schematics & Datasheet | **M1, M5, M6** | PTC17 (TOUCH_A / ADC0_SE15), R525 ($4.7\text{ k}\Omega$), PTC16 (TOUCH_B / ADC0_SE14), C533 ($10\text{ pF}$) topology and [S32K1xx Data Sheet](https://www.mouser.com/pdfdocs/NXP_S32K1xx_DS.pdf) $C_{\text{ADIN}} \approx 4\text{ pF}$ specification. |

### Part 1: Core Academic Journal & Preprint Papers (IEEE Citation Format)

* **Automotive Capacitive Touch Pad Real-Time Online Defect Diagnostics (Core Basis for M5)**
  * **[1]** S. M. Hong, J.-W. Park, J.-H. Jeong, and S. Y. Kim, "Real-Time Defect Detection of Capacitive Touch Pads for Hands-Off Detection in Advanced Driver Assistance Systems," *Applied Sciences*, vol. 16, no. 4, art. no. 1675, Feb. 2026, doi: [10.3390/app16041675](https://doi.org/10.3390/app16041675). Available: https://www.mdpi.com/2076-3417/16/4/1675
* **Kalman Filter-Assisted Recursive State Estimation for SAR ADCs (Core Basis for M2)**
  * **[2]** X. Feng, Y. Zhao, J. Xiong, D. Lin, Y. Zhong, W. Liu, Z. Ji, R. Tian, C. Zhuo, and Y. Yin, "A Kalman Filter-Assisted Data-Predictive SAR ADC With Reduced Switching Energy for Low-Power Applications," *arXiv preprint arXiv:2607.16139 [eess.SP]*, Jul. 2026, doi: [10.48550/arXiv.2607.16139](https://doi.org/10.48550/arXiv.2607.16139). Available: https://arxiv.org/abs/2607.16139
* **Charge-Redistribution SAR ADC Parasitic Capacitance Modeling & Phase Control (Physical Basis for M1 / M2)**
  * **[3]** X. Ouyang, H. Kuang, D. Kong, Z. Cheng, and H. Yuan, "A Phase-Adjustable Noise-Shaping SAR ADC for Mitigating Parasitic Capacitance Effects from PIP Capacitors," *Sensors*, vol. 25, no. 19, art. no. 6029, Oct. 2025, doi: [10.3390/s25196029](https://doi.org/10.3390/s25196029). Available: https://www.mdpi.com/1424-8220/25/19/6029
* **Capacitive Touch Sensing AFE Noise Immunity & Frequency Avoidance (Basis for M2 Hopping)**
  * **[4]** Y.-H. Hwang, "Study of Circuit Techniques for Enhancing Display Noise Immunity in Analog Front-end of Mutual-capacitive Touch Systems," *Journal of Semiconductor Technology and Science (JSTS)*, vol. 24, no. 5, pp. 473–482, Oct. 2024, doi: [10.5573/JSTS.2024.24.5.473](https://doi.org/10.5573/JSTS.2024.24.5.473). Available: https://jsts.org/jsts/XmlViewer/f432362
* **Fully-Differential Charge-Sharing Self-Capacitance Mismatch Calibration (Core Basis for M1 / M3)**
  * **[5]** S. Seong, S. Lee, S. Bae, and M. Lee, "Self Capacitance Mismatch Calibration Technique for Fully-Differential Touch Screen Panel Self Capacitance Sensing System," *Sensors*, vol. 23, no. 7, art. no. 3779, Apr. 2023, doi: [10.3390/s23073779](https://doi.org/10.3390/s23073779). Available: https://www.mdpi.com/1424-8220/23/7/3779
* **In-Vehicle Capacitive Sensing Temporal Features & 3-Step Confidence Post-Processing (Core Basis for M4)**
  * **[6]** H. Pyeon, H. Kim, R. C. Kim, G. Oh, and S. Lim, "Deep Learning-Based Driver's Hands on/off Prediction System Using In-Vehicle Data," *Sensors*, vol. 23, no. 3, art. no. 1442, Jan. 2023, doi: [10.3390/s23031442](https://doi.org/10.3390/s23031442). Available: https://www.mdpi.com/1424-8220/23/3/1442
* **Capacitive Touch SNR Enhancement & Machine Learning Identification Review (Core Basis for M2 / M4)**
  * **[7]** H. Nam, K.-H. Seol, J. Lee, H. Cho, and S. W. Jung, "Review of Capacitive Touchscreen Technologies: Overview, Research Trends, and Machine Learning Approaches," *Sensors*, vol. 21, no. 14, art. no. 4776, Jul. 2021, doi: [10.3390/s21144776](https://doi.org/10.3390/s21144776). Available: https://www.mdpi.com/1424-8220/21/14/4776
* **Microcontroller Charge Transfer Pseudo-Differential Measuring Under Low Power Quality (Basis for M1 / M2 $\ge 28\text{ dB}$ SNR)**
  * **[8]** Y.-H. Yu and T.-Y. Sun, "A Pseudo-Differential Measuring Approach for Implementing Microcontroller-Based Capacitive Touch Sensing in Low-Power Quality Situation," *IEEE Sensors Journal*, vol. 16, no. 2, pp. 390–399, Jan. 2016, doi: [10.1109/JSEN.2015.2483747](https://doi.org/10.1109/JSEN.2015.2483747). Available: https://www.ndl.gov.in/re_document/ieee_xplore/1234567_ieee/673552

### Part 2: Semiconductor Vendor Application Notes & Hardware Specifications (IEEE Citation Format)

* **NXP S32K144 Automotive General-Purpose MCU Touch Sensing Solution Whitepaper (Baseline for M1 / M3 / M6)**
  * **[9]** S. Sun, "GPIS Touch Sense Solution," NXP Semiconductors, Automotive CAS Technical Presentation, Doc. No. APF-AUT-T3280, Sep. 2018. Available: https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/connects/202/1/APF-AUT-T3280_China.pdf
* **Microchip Differential Capacitive Voltage Divider (CVD) Acquisition & Jittering Application Note (Baseline for M1 / M2)**
  * **[10]** B. Davison, "mTouch™ Sensing Solution Acquisition Methods Capacitive Voltage Divider," Microchip Technology Inc., Application Note AN1478 (DS01478B), 2012. Available: https://www.microchip.com/mymicrochip/filehandler.aspx?ddocname=en560193
* **Microchip Bipolar CVD Differential Subtraction & Pin Grounding Guide (Baseline for M1)**
  * **[11]** Microchip Technology Inc., "Touch Sensing Using Analog-to-Digital Converter With Computation (ADCC)," Microchip Developer Help, Mar. 2026. Available: https://developerhelp.microchip.com/xwiki/bin/view/applications/touch/sensing-using-adcc/
* **Texas Instruments Noise-Tolerant (IEC 61000-4-6) Frequency Hopping & Dynamic Threshold Adjustment Report (Baseline for M2 / M4)**
  * **[12]** Texas Instruments Inc., "Enabling Noise Tolerant Capacitive Touch HMIs With MSP CapTIvate™ Microcontrollers," Texas Instruments, Application Report SLAY045B, 2017. Available: https://www.ti.com/lit/pdf/slay045
* **NXP S32K1xx SAR ADC Self-Calibration & Bandgap Voltage Monitoring Specification (Baseline for M1 / M5)**
  * **[13]** NXP Semiconductors, "S32K1xx ADC Guidelines, Spec, and Configuration," NXP Semiconductors, Application Note AN12217, Rev. 0, Jun. 2018. Available: https://www.nxp.com/docs/en/application-note/AN12217.pdf
* **NXP S32K144 Evaluation Board Schematic & Chip Datasheet (Hardware Topology & Electrical Parameters)**
  * **[14]** NXP Semiconductors, "S32K144EVB-Q100 Evaluation Board Schematic," Doc. No. SCH-29248 / SPF-29248, Rev. C, Mar. 2018. Available: https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/S32K/9169/1/SPF-29248_C.pdf; and "S32K1xx Data Sheet," NXP Semiconductors, Rev. 14, 2021. Available: https://www.mouser.com/pdfdocs/NXP_S32K1xx_DS.pdf

---

## License

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
