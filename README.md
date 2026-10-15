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

- [x] **M1: Bipolar Differential Physical Layer** (`v1.0.1-m1`) - **[HW] static only**
  - [x] ADC0 32x self-calibration (AN12217), Bandgap Ch27 monitoring. [HW]
  - [x] Bipolar 4-phase CVD acquisition (`Touch_AcquireSingleDiffCVD`). [HW]
  - [x] Untouched workpoint A = 2079.2 (sigma 0.46), B = 1745.0 (sigma 0.30), delta_D = -334.1 (sigma 0.59), 100 samples. [HW]
  - [ ] "Touch delta 80~200 LSB" criterion: the M1 dataset contains **no touched samples**; first real touch data came later (about +1400 to +2700 LSB, i.e. far above the planned range). [see Known Issues #3]
- [~] **M2: Spread-Spectrum Hopping & Cascaded Kalman Filter** (`v1.0.2-m2`) - **[SIM] + partial [HW]**
  - [x] 16-bit LFSR, coprime delays {15,23,31}, N=9 burst, alpha-trimmed mean, adaptive 1D Kalman. [code present, runs on HW]
  - [ ] Static noise: **[HW] measured x_est sigma = 1.6-2.3 LSB (target < 1.5)**. The earlier "0.103 LSB / 65.57 dB SNR" was **[SIM] only** and is not a hardware result.
  - [x] Tap response: large consistent steps now bypass Kalman averaging (fast path added after hardware test). [HW: touch detected within 1 frame]
  - [ ] EFT +800 LSB immunity: **[SIM] only**, never injected on hardware.
- [~] **M3: Asymmetric Baseline Tracker** (`v1.0.3-m3`) - **[SIM] + [HW] baseline stable**
  - [x] 4-zone state machine, slew +1.5/-2.0 LSB/s. Baseline stayed at about -338..-340 across all hardware recordings. [HW]
  - [ ] Touch-on-boot 285 ms, 5 s slow approach, hair-dryer drift, 15 s coin timeout: **[SIM] only**.
- [x] **M4: Temporal Envelope Classification (TinyML) & Water Rejection** (`v1.0.4-m4-rev2`) - **[SIM] 100% Pass + [HW] Integrated**
  - [x] 4 time-domain envelope features: 2-cycle attack slope, linear dynamic SNR, normalized curvature variance, sustain ratio. [HW & SIM]
  - [x] Dynamic Threshold Adjustment (DTA: 34.0 ~ 58.0 LSB) accommodates 2mm acrylic / glove while rejecting water mist. [HW & SIM]
  - [x] 3-step confidence sliding window hysteresis (0.64 trigger / 0.38 release) with soft-margin scoring. [HW & SIM]
  - [x] Zone 2 8-second approach timeout anti-lockup implemented in `touch_baseline.c`. [HW & SIM]
  - [x] `tests/test_m4_contract.py`: 140-trial validation passes 100% (Bare Finger TPR 100%, Glove/Acrylic TPR 100%, Water FPR 0%, Swipe FPR 0%, Reaction Latency 39.9 ms). [SIM]
  - [x] Hardware verification: builds with 0 errors/0 warnings in GCC 10.2, flashed to EVB, streaming 66.8 Hz live telemetry (`data/hardware/m_post_expert_test.csv`). [HW]
- [~] **M5: Dual-Channel Diagnostics & ASIL-B Fail-Safe** (`v1.0.5-m5`) - **[SIM] + [HW] healthy-state only**
  - [x] Healthy-state `Diag:OK` with zero false fail-safes on hardware. [HW]
  - [ ] Short-to-GND/VDD, R525 open, VREFH drift fault injection: **[SIM] only**, never physically injected.
- [~] **M6: VLPS Low-Power & 12 h Burn-in** (in progress, **not signed off**)
  - [x] Unified driver `touch_sense.h/.c` written and builds clean. [builds; not used by `main.c` yet]
  - [ ] Execution time: **[HW] measured about 210 us per cycle (194-235 us) vs. target <= 60 us - FAIL.** An earlier "28-35 us" figure was an estimate, not a measurement.
  - [ ] VLPS current 45-70 uA: **[NONE]** - never measured (needs ammeter on J15). VLPS entry itself has not been exercised on hardware.
  - [ ] 12 h burn-in: **[NONE]** - not run. An earlier "2,880,000-cycle simulation" plan was only a plan.

---

## Real Hardware Data (for review)

All files are raw UART captures from the real board (66.7 Hz, one frame per 15 ms), recorded with
[`tools/record_live.py`](tools/record_live.py). Columns: `t_s,cycle,A,B,deltaD,xest,base,dyn,cls,conf,touch,exec_us,status`
(`cls`: 0 none, 1 glove, 2 bare finger, 3 water, 4 edge hover).

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
6. **Execution time about 210 us vs. 60 us target (M6).** Measured with the SysTick down-counter around acquisition + filter + baseline + classifier + diagnostics (UART output excluded). Needs profiling (float math, 9-sample burst with `NopDelayExact`).
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

## License

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
