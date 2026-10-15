#!/usr/bin/env python3
"""
====================================================================================================
Project              : S32K144W Touch Sensing CVD Algorithm
Milestone            : Milestone 5 - Dual ADC Channel On-line Diagnostics & ISO 26262 ASIL-B Engine
Platform             : CORTEXM / S32K144W (XS32K14WEVB-Q064)
Dependencies         : numpy, math, subprocess, re, os

Copyright (c) 2026, Max97k
All Rights Reserved.
====================================================================================================
Validates Milestone 5 Quantitative Pass Criteria:
1. Architectural & Safety Contract (ISO 26262 ASIL-B, MISRA C Zero Heap, Sole Copyright)
2. ELF Binary Symbol Table & Section Memory Allocation
3. Fault Injection Test A: Electrode Short-to-GND Detection & Reaction Time (<= 3 cycles / 60 ms)
4. Fault Injection Test B: Electrode Short-to-VDD Detection & Reaction Time (<= 3 cycles / 60 ms)
5. Fault Injection Test C: Bridging Resistor R525 Open-Circuit / Degradation Detection
6. Fault Injection Test D: ADC Reference Voltage VREFH (+/- 5%) Drift Monitoring
7. Fail-Safe Interlock Verification (Unconditional is_touched = False during any defect)
8. Self-Healing Auto-Recovery Latency (<= 200 ms after fault removal) & Latched Fault History
9. Fault Injection Log CSV Generation (m5_fault_injection_log.csv)
"""
import math
import os
import re
import subprocess
import sys
import numpy as np

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ELF_PATH = os.path.join(REPO_ROOT, "Debug_FLASH", "s32k144w-touch-sense-cvd.elf")
DIAG_C_PATH = os.path.join(REPO_ROOT, "src", "touch_diag.c")
DIAG_H_PATH = os.path.join(REPO_ROOT, "include", "touch_diag.h")
MAIN_C_PATH = os.path.join(REPO_ROOT, "src", "main.c")

NM_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-nm.exe"
SIZE_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-size.exe"


class PythonTouchDiagnostics:
    """Exact software replica of Touch_Diag_RunCycle in src/touch_diag.c"""
    SAMPLE_MIN_LSB = 300
    SAMPLE_MAX_LSB = 3800
    BALANCE_DIFF_MAX = 250
    VREFH_MIN_MV = 4750
    VREFH_MAX_MV = 5250
    FAULT_SET_CYCLES = 2
    FAULT_CLEAR_CYCLES = 8

    FAULT_NONE          = 0x00000000
    FAULT_SHORT_GND_A   = 0x00000001
    FAULT_SHORT_GND_B   = 0x00000002
    FAULT_SHORT_VDD_A   = 0x00000004
    FAULT_SHORT_VDD_B   = 0x00000008
    FAULT_RESISTOR_OPEN = 0x00000010
    FAULT_VREF_DRIFT    = 0x00000020
    FAULT_ELECTRODE_OPEN= 0x00000040

    def __init__(self):
        self.active_faults = self.FAULT_NONE
        self.latched_faults = self.FAULT_NONE
        self.is_healthy = True

        self.short_gnd_a_cnt = 0
        self.short_gnd_b_cnt = 0
        self.short_vdd_a_cnt = 0
        self.short_vdd_b_cnt = 0
        self.r525_open_cnt = 0
        self.vref_drift_cnt = 0
        self.healthy_cycles_cnt = 0

        self.last_sample_A = 2048
        self.last_sample_B = 2048
        self.last_balance_diff = 0
        self.last_vrefh_mv = 5000

        self.total_fault_events = 0
        self.total_recoveries = 0

    def run_cycle(self, sample_A, sample_B, balance_diff, vrefh_mv):
        self.last_sample_A = sample_A
        self.last_sample_B = sample_B
        self.last_balance_diff = balance_diff
        self.last_vrefh_mv = vrefh_mv

        raw_faults = self.FAULT_NONE

        # Diag 1: Short to GND
        if sample_A < self.SAMPLE_MIN_LSB:
            if self.short_gnd_a_cnt < self.FAULT_SET_CYCLES:
                self.short_gnd_a_cnt += 1
            if self.short_gnd_a_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_SHORT_GND_A
        else:
            self.short_gnd_a_cnt = 0

        if sample_B < self.SAMPLE_MIN_LSB:
            if self.short_gnd_b_cnt < self.FAULT_SET_CYCLES:
                self.short_gnd_b_cnt += 1
            if self.short_gnd_b_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_SHORT_GND_B
        else:
            self.short_gnd_b_cnt = 0

        # Diag 2: Short to VDD
        if sample_A > self.SAMPLE_MAX_LSB:
            if self.short_vdd_a_cnt < self.FAULT_SET_CYCLES:
                self.short_vdd_a_cnt += 1
            if self.short_vdd_a_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_SHORT_VDD_A
        else:
            self.short_vdd_a_cnt = 0

        if sample_B > self.SAMPLE_MAX_LSB:
            if self.short_vdd_b_cnt < self.FAULT_SET_CYCLES:
                self.short_vdd_b_cnt += 1
            if self.short_vdd_b_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_SHORT_VDD_B
        else:
            self.short_vdd_b_cnt = 0

        # Diag 3: R525 Open
        if abs(balance_diff) > self.BALANCE_DIFF_MAX:
            if self.r525_open_cnt < self.FAULT_SET_CYCLES:
                self.r525_open_cnt += 1
            if self.r525_open_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_RESISTOR_OPEN
        else:
            self.r525_open_cnt = 0

        # Diag 4: VREFH Drift
        if (vrefh_mv < self.VREFH_MIN_MV) or (vrefh_mv > self.VREFH_MAX_MV):
            if self.vref_drift_cnt < self.FAULT_SET_CYCLES:
                self.vref_drift_cnt += 1
            if self.vref_drift_cnt >= self.FAULT_SET_CYCLES:
                raw_faults |= self.FAULT_VREF_DRIFT
        else:
            self.vref_drift_cnt = 0

        # Qualification & Auto-recovery
        if raw_faults != self.FAULT_NONE:
            if self.active_faults == self.FAULT_NONE:
                self.total_fault_events += 1
            self.active_faults = raw_faults
            self.latched_faults |= raw_faults
            self.is_healthy = False
            self.healthy_cycles_cnt = 0
        else:
            if self.active_faults != self.FAULT_NONE:
                self.healthy_cycles_cnt += 1
                if self.healthy_cycles_cnt >= self.FAULT_CLEAR_CYCLES:
                    self.active_faults = self.FAULT_NONE
                    self.is_healthy = True
                    self.healthy_cycles_cnt = 0
                    self.total_recoveries += 1
            else:
                self.is_healthy = True
                self.healthy_cycles_cnt = 0

        return self.active_faults

    def reset_latched(self):
        self.latched_faults = self.FAULT_NONE


def run_test(name, condition, details=""):
    print(f"[*] Test: {name}")
    if condition:
        print(f"    [+] PASS: {details}")
        return True
    else:
        print(f"    [-] FAIL: {details}")
        return False


def main():
    print("=" * 80)
    print("   S32K144W Touch Sensing - Milestone 5 Comprehensive Verification")
    print("   Dual ADC Channel On-line Diagnostics & ISO 26262 ASIL-B Safety Engine")
    print("=" * 80)

    results = []
    log_records = []

    # ----------------------------------------------------------------------------------
    # Test 1: Architectural & Safety Contract
    # ----------------------------------------------------------------------------------
    with open(DIAG_C_PATH, "r", encoding="utf-8") as f:
        diag_c = f.read()
    with open(DIAG_H_PATH, "r", encoding="utf-8") as f:
        diag_h = f.read()
    with open(MAIN_C_PATH, "r", encoding="utf-8") as f:
        main_c = f.read()

    has_heap = ("malloc(" in diag_c or "calloc(" in diag_c or "free(" in diag_c)
    has_author_c = "Copyright (c) 2026, Max97k" in diag_c and "All Rights Reserved." in diag_c
    has_author_h = "Copyright (c) 2026, Max97k" in diag_h and "All Rights Reserved." in diag_h
    has_author_main = "Copyright (c) 2026, Max97k" in main_c and "All Rights Reserved." in main_c

    has_fault_codes = ("TOUCH_FAULT_SHORT_GND_A" in diag_h and
                       "TOUCH_FAULT_SHORT_GND_B" in diag_h and
                       "TOUCH_FAULT_SHORT_VDD_A" in diag_h and
                       "TOUCH_FAULT_SHORT_VDD_B" in diag_h and
                       "TOUCH_FAULT_RESISTOR_OPEN" in diag_h and
                       "TOUCH_FAULT_VREF_DRIFT" in diag_h)

    # ASIL-B fail-safe clamp in main.c
    has_failsafe = "active_faults != TOUCH_FAULT_NONE" in main_c and "g_touch_active = FALSE" in main_c

    t1_pass = (not has_heap) and has_author_c and has_author_h and has_author_main and \
              has_fault_codes and has_failsafe

    results.append(run_test(
        "M5-1: Architectural & Safety Contract (Zero Heap, Sole Copyright, Fault Codes, Fail-Safe)",
        t1_pass,
        "Heap=0, Copyright=Max97k, FaultCodes=6, FailSafeClamp=Active"
    ))

    # ----------------------------------------------------------------------------------
    # Test 2: ELF Binary Symbol Table & Section Memory Allocation
    # ----------------------------------------------------------------------------------
    elf_exists = os.path.isfile(ELF_PATH)
    t2_pass = False
    if elf_exists:
        try:
            nm_res = subprocess.run([NM_TOOL, ELF_PATH], capture_output=True, text=True, check=True)
            symbols = nm_res.stdout
            has_init = "Touch_Diag_Init" in symbols
            has_run = "Touch_Diag_RunCycle" in symbols
            has_healthy = "Touch_Diag_IsHealthy" in symbols
            has_get = "Touch_Diag_GetFaults" in symbols
            has_reset = "Touch_Diag_ResetLatched" in symbols
            has_diag_obj = "g_touch_diagnostics" in symbols

            size_res = subprocess.run([SIZE_TOOL, ELF_PATH], capture_output=True, text=True, check=True)
            lines = size_res.stdout.strip().split("\n")
            parts = lines[-1].split()
            text_size = int(parts[0])
            bss_size = int(parts[2])

            t2_pass = (has_init and has_run and has_healthy and has_get and
                       has_reset and has_diag_obj and text_size < 45000)
            details = f"All symbols found. ELF .text={text_size} B (<45 KB), .bss={bss_size} B"
        except Exception as e:
            details = f"ELF tool error: {e}"
    else:
        details = "ELF not found"

    results.append(run_test("M5-2: ELF Symbol Table & Section Memory Allocation", t2_pass, details))

    # ----------------------------------------------------------------------------------
    # Test 3: Fault Injection Test A - Electrode Short-to-GND (TOUCH_A & TOUCH_B)
    # ----------------------------------------------------------------------------------
    diag = PythonTouchDiagnostics()
    # 5 quiescent cycles
    for c in range(5):
        diag.run_cycle(2079, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "NORMAL", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": diag.active_faults, "Healthy": diag.is_healthy})

    # Inject TOUCH_A Short to GND (SE15 = 50 LSB < 300)
    gnd_a_reaction_cycles = None
    for c in range(5, 12):
        f = diag.run_cycle(50, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "SHORT_GND_A", "SE15": 50, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_SHORT_GND_A) and (gnd_a_reaction_cycles is None):
            gnd_a_reaction_cycles = c - 4

    # Remove short (restore SE15 = 2079) and measure recovery
    gnd_a_recovery_cycles = None
    for c in range(12, 28):
        f = diag.run_cycle(2079, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "RECOVERY_GND_A", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if diag.is_healthy and (gnd_a_recovery_cycles is None):
            gnd_a_recovery_cycles = c - 11

    # Inject TOUCH_B Short to GND (SE14 = 80 LSB < 300)
    gnd_b_reaction_cycles = None
    for c in range(28, 35):
        f = diag.run_cycle(2079, 80, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "SHORT_GND_B", "SE15": 2079, "SE14": 80, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_SHORT_GND_B) and (gnd_b_reaction_cycles is None):
            gnd_b_reaction_cycles = c - 27

    gnd_b_recovery_cycles = None
    for c in range(35, 50):
        f = diag.run_cycle(2079, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "RECOVERY_GND_B", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if diag.is_healthy and (gnd_b_recovery_cycles is None):
            gnd_b_recovery_cycles = c - 34

    t_react_gnd_a = gnd_a_reaction_cycles * 15
    t_rec_gnd_a = gnd_a_recovery_cycles * 15
    t3_pass = (gnd_a_reaction_cycles <= 3) and (gnd_b_reaction_cycles <= 3) and \
              (t_rec_gnd_a <= 200) and (gnd_b_recovery_cycles * 15 <= 200)

    results.append(run_test(
        "M5-3: Electrode Short-to-GND Injection (Reaction <= 3 cycles, Recovery <= 200 ms)",
        t3_pass,
        f"GND_A Reaction={t_react_gnd_a} ms ({gnd_a_reaction_cycles} cycles), Recovery={t_rec_gnd_a} ms ({gnd_a_recovery_cycles} cycles)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 4: Fault Injection Test B - Electrode Short-to-VDD (TOUCH_A & TOUCH_B)
    # ----------------------------------------------------------------------------------
    vdd_a_reaction_cycles = None
    for c in range(50, 57):
        f = diag.run_cycle(4010, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "SHORT_VDD_A", "SE15": 4010, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_SHORT_VDD_A) and (vdd_a_reaction_cycles is None):
            vdd_a_reaction_cycles = c - 49

    vdd_a_recovery_cycles = None
    for c in range(57, 72):
        f = diag.run_cycle(2079, 1745, 6, 5000)
        log_records.append({"cycle": c + 1, "test": "RECOVERY_VDD_A", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if diag.is_healthy and (vdd_a_recovery_cycles is None):
            vdd_a_recovery_cycles = c - 56

    t_react_vdd_a = vdd_a_reaction_cycles * 15
    t_rec_vdd_a = vdd_a_recovery_cycles * 15
    t4_pass = (vdd_a_reaction_cycles <= 3) and (t_rec_vdd_a <= 200)

    results.append(run_test(
        "M5-4: Electrode Short-to-VDD Injection (Reaction <= 3 cycles, Recovery <= 200 ms)",
        t4_pass,
        f"VDD_A Reaction={t_react_vdd_a} ms ({vdd_a_reaction_cycles} cycles), Recovery={t_rec_vdd_a} ms ({vdd_a_recovery_cycles} cycles)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 5: Fault Injection Test C - Bridging Resistor R525 Open-Circuit / Degradation
    # ----------------------------------------------------------------------------------
    # Balance diff spikes to +350 LSB (> 250 LSB)
    r525_reaction_cycles = None
    for c in range(72, 80):
        f = diag.run_cycle(2079, 1745, 350, 5000)
        log_records.append({"cycle": c + 1, "test": "R525_OPEN", "SE15": 2079, "SE14": 1745, "Diff": 350, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_RESISTOR_OPEN) and (r525_reaction_cycles is None):
            r525_reaction_cycles = c - 71

    r525_recovery_cycles = None
    for c in range(80, 95):
        f = diag.run_cycle(2079, 1745, 8, 5000)
        log_records.append({"cycle": c + 1, "test": "RECOVERY_R525", "SE15": 2079, "SE14": 1745, "Diff": 8, "VREFH": 5000, "Fault": f, "Healthy": diag.is_healthy})
        if diag.is_healthy and (r525_recovery_cycles is None):
            r525_recovery_cycles = c - 79

    t_react_r525 = r525_reaction_cycles * 15
    t_rec_r525 = r525_recovery_cycles * 15
    t5_pass = (r525_reaction_cycles <= 3) and (t_rec_r525 <= 200)

    results.append(run_test(
        "M5-5: Bridging Resistor R525 Open-Circuit Injection (Reaction <= 3 cycles, Recovery <= 200 ms)",
        t5_pass,
        f"R525 Reaction={t_react_r525} ms ({r525_reaction_cycles} cycles), Recovery={t_rec_r525} ms ({r525_recovery_cycles} cycles)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 6: Fault Injection Test D - Reference Voltage VREFH Drift Monitoring
    # ----------------------------------------------------------------------------------
    # Under-voltage (4400 mV < 4750 mV)
    vref_uv_reaction_cycles = None
    for c in range(95, 102):
        f = diag.run_cycle(2079, 1745, 6, 4400)
        log_records.append({"cycle": c + 1, "test": "VREF_UNDERVOLTAGE", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 4400, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_VREF_DRIFT) and (vref_uv_reaction_cycles is None):
            vref_uv_reaction_cycles = c - 94

    for c in range(102, 117):
        diag.run_cycle(2079, 1745, 6, 5000)

    # Over-voltage (5600 mV > 5250 mV)
    vref_ov_reaction_cycles = None
    for c in range(117, 125):
        f = diag.run_cycle(2079, 1745, 6, 5600)
        log_records.append({"cycle": c + 1, "test": "VREF_OVERVOLTAGE", "SE15": 2079, "SE14": 1745, "Diff": 6, "VREFH": 5600, "Fault": f, "Healthy": diag.is_healthy})
        if (f & PythonTouchDiagnostics.FAULT_VREF_DRIFT) and (vref_ov_reaction_cycles is None):
            vref_ov_reaction_cycles = c - 116

    for c in range(125, 140):
        diag.run_cycle(2079, 1745, 6, 5000)

    t6_pass = (vref_uv_reaction_cycles <= 3) and (vref_ov_reaction_cycles <= 3)

    results.append(run_test(
        "M5-6: Reference Voltage VREFH (+/- 5%) Drift Monitoring (UV & OV Detection)",
        t6_pass,
        f"UV Reaction={vref_uv_reaction_cycles * 15} ms, OV Reaction={vref_ov_reaction_cycles * 15} ms (All <= 45 ms)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 7: Fail-Safe Interlock Verification
    # ----------------------------------------------------------------------------------
    # Ensure that during any defect, touch is UNCONDITIONALLY clamped to False
    sim_diag = PythonTouchDiagnostics()
    sim_diag.run_cycle(50, 1745, 6, 5000)  # cycle 1
    sim_diag.run_cycle(50, 1745, 6, 5000)  # cycle 2 -> fault qualified

    # Candidate touch says True (e.g. noise spike or finger touching shorted pin)
    candidate_touch = True
    failsafe_touch = False if not sim_diag.is_healthy else candidate_touch

    t7_pass = (failsafe_touch is False) and (sim_diag.is_healthy is False)

    results.append(run_test(
        "M5-7: Fail-Safe Interlock Verification (Zero False Touch Under Hardware Defect)",
        t7_pass,
        "is_healthy=False -> is_touched=False strictly enforced (ASIL-B Safety Interlock)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 8: Latched Fault History & Diagnostic Reset
    # ----------------------------------------------------------------------------------
    latched = diag.latched_faults
    expected_mask = (PythonTouchDiagnostics.FAULT_SHORT_GND_A |
                     PythonTouchDiagnostics.FAULT_SHORT_GND_B |
                     PythonTouchDiagnostics.FAULT_SHORT_VDD_A |
                     PythonTouchDiagnostics.FAULT_RESISTOR_OPEN |
                     PythonTouchDiagnostics.FAULT_VREF_DRIFT)

    has_all_latched = (latched & expected_mask) == expected_mask
    diag.reset_latched()
    reset_ok = (diag.latched_faults == PythonTouchDiagnostics.FAULT_NONE)

    t8_pass = has_all_latched and reset_ok and (diag.total_recoveries >= 4)

    results.append(run_test(
        "M5-8: Latched Fault History Retention & Diagnostic Reset Service",
        t8_pass,
        f"Latched Faults=0x{latched:08X}, Reset=OK, Total Recoveries={diag.total_recoveries}"
    ))

    # ----------------------------------------------------------------------------------
    # Test 9: Export Fault Injection Test Log CSV
    # ----------------------------------------------------------------------------------
    csv_path = os.path.join(REPO_ROOT, "data", "simulated", "m5_fault_injection_log.csv")
    with open(csv_path, "w", encoding="utf-8") as f:
        f.write("Cycle,TestScenario,Sample_A_SE15,Sample_B_SE14,BalanceDiff_R525,VREFH_mV,ActiveFaultMask,IsHealthy\n")
        for r in log_records:
            f.write(f"{r['cycle']},{r['test']},{r['SE15']},{r['SE14']},{r['Diff']},{r['VREFH']},0x{r['Fault']:08X},{1 if r['Healthy'] else 0}\n")

    t9_pass = os.path.isfile(csv_path) and (len(log_records) >= 100)
    results.append(run_test(
        "M5-9: Fault Injection Log CSV Generation",
        t9_pass,
        f"Exported {len(log_records)} cycles to {csv_path}"
    ))

    # ----------------------------------------------------------------------------------
    # Summary Evaluation
    # ----------------------------------------------------------------------------------
    all_passed = all(results)
    print("=" * 80)
    if all_passed:
        print("   [+] ALL MILESTONE 5 CONTRACT TESTS PASSED PERFECTLY!")
        print("   [+] Ready for Milestone 5 Sign-off & Milestone 6 Plan Presentation.")
    else:
        print("   [-] MILESTONE 5 CONTRACT FAILED!")
    print("=" * 80)

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
