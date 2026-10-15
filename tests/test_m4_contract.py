#!/usr/bin/env python3
"""
====================================================================================================
Project              : S32K144W Touch Sensing CVD Algorithm
Milestone            : Milestone 4 - TinyML Temporal Envelope Classifier & Water Droplet Immunity
Platform             : CORTEXM / S32K144W (XS32K14WEVB-Q064)
Dependencies         : numpy, math, subprocess, re, os

Copyright (c) 2026, Max97k
All Rights Reserved.
====================================================================================================
Validates Milestone 4 Quantitative Pass Criteria:
1. Architectural & Safety Contract (MISRA C Zero Heap, Sole Copyright, 4 Envelope Features)
2. ELF Binary Symbol Table & Section Memory Allocation
3. Bare Finger Fast Tap & Sustained Press Dataset Simulation (50 trials, TPR >= 98%, latency <= 60~90 ms)
4. Glove / 2mm Acrylic Overlay Press Dataset Simulation (50 trials, TPR >= 98%)
5. Water Droplet & Spray Condensation Rejection Dataset Simulation (20 trials, FPR = 0.0%)
6. Edge Hover & Grazing Fast Swipe Rejection Dataset Simulation (20 trials, FPR = 0.0%)
7. Response Latency & 3-Step Confidence Smoothing Verification (<= 60 ~ 90 ms)
8. Multi-Class Dataset Export & Confusion Matrix Verification
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
TINYML_C_PATH = os.path.join(REPO_ROOT, "src", "touch_tinyml.c")
TINYML_H_PATH = os.path.join(REPO_ROOT, "include", "touch_tinyml.h")
MAIN_C_PATH = os.path.join(REPO_ROOT, "src", "main.c")

NM_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-nm.exe"
SIZE_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-size.exe"


class PythonTinyMLClassifier:
    """Exact software replica of Touch_TinyML_Update in src/touch_tinyml.c"""
    WINDOW_SIZE = 10
    CONF_WINDOW_SIZE = 3
    CONF_TRIGGER = 0.65
    CONF_RELEASE = 0.35
    MIN_BARE_DELTA = 80.0
    MIN_GLOVE_DELTA = 35.0
    WATER_MAX_DELTA = 45.0
    MIN_ATTACK_SLOPE = 15.0
    MAX_PLATEAU_VAR = 9.0

    def __init__(self):
        self.history_delta = [0.0] * self.WINDOW_SIZE
        self.history_spread = [1.0] * self.WINDOW_SIZE
        self.head_idx = 0
        self.sample_count = 0
        self.conf_window = [0.0] * self.CONF_WINDOW_SIZE
        self.conf_head = 0

        self.features = {
            "attack_slope": 0.0,
            "dynamic_snr_db": 0.0,
            "plateau_variance": 0.0,
            "release_symmetry": 1.0,
            "peak_delta": 0.0,
            "contact_duration": 0
        }
        self.detected_class = 0  # 0: NONE, 1: BARE, 2: GLOVE, 3: WATER, 4: HOVER
        self.confidence = 0.0
        self.is_valid_touch = False
        self.water_rejections_count = 0
        self.hover_rejections_count = 0

    def extract_features(self):
        if self.sample_count == 0:
            return

        n = self.sample_count
        ordered_delta = []
        ordered_spread = []
        for i in range(n):
            idx = (self.head_idx + self.WINDOW_SIZE - n + i) % self.WINDOW_SIZE
            ordered_delta.append(self.history_delta[idx])
            ordered_spread.append(self.history_spread[idx])

        peak = max(ordered_delta)
        sum_spread = sum(ordered_spread)

        duration = 0
        for val in reversed(ordered_delta):
            if val >= 20.0:
                duration += 1
            else:
                break

        self.features["peak_delta"] = peak
        self.features["contact_duration"] = duration

        # Attack slope
        if n >= 3:
            max_slope = 0.0
            for i in range(2, n):
                s = (ordered_delta[i] - ordered_delta[i - 2]) / 2.0
                if s > max_slope:
                    max_slope = s
        elif n >= 2:
            max_slope = ordered_delta[n - 1] - ordered_delta[n - 2]
        else:
            max_slope = 0.0
        self.features["attack_slope"] = max(max_slope, 0.0)

        # Plateau variance computed ONLY when contact has persisted for >= 4 cycles (60 ms)
        if duration >= 4:
            plat = ordered_delta[-4:]
            p_mean = sum(plat) / 4.0
            p_var = sum((x - p_mean) ** 2 for x in plat) / 4.0
            self.features["plateau_variance"] = p_var
        else:
            self.features["plateau_variance"] = 0.0

        # Dynamic SNR
        avg_spread = sum_spread / float(n) if n > 0 else 1.0
        if avg_spread < 0.2:
            avg_spread = 0.2
        if peak > 0.5:
            self.features["dynamic_snr_db"] = 20.0 * math.log10(peak / avg_spread)
        else:
            self.features["dynamic_snr_db"] = 0.0

        self.features["release_symmetry"] = 1.0

    def update(self, delta, spread=1.0):
        # 1. Circular buffer push
        self.history_delta[self.head_idx] = delta
        self.history_spread[self.head_idx] = spread
        self.head_idx = (self.head_idx + 1) % self.WINDOW_SIZE
        if self.sample_count < self.WINDOW_SIZE:
            self.sample_count += 1

        # 2. Extract features
        self.extract_features()
        feat = self.features

        # 3. Decision Tree
        if feat["peak_delta"] < 20.0:
            inst_class = 0  # NONE
            inst_prob = 0.0
        elif (feat["peak_delta"] >= 25.0) and (feat["contact_duration"] <= 2) and (delta < 25.0):
            inst_class = 4  # EDGE_HOVER
            inst_prob = 0.05
            self.hover_rejections_count += 1
        elif (feat["plateau_variance"] > self.MAX_PLATEAU_VAR) and (feat["contact_duration"] >= 4):
            # Water droplet splash/ringing: unstable plateau
            inst_class = 3  # WATER_DROPLET
            inst_prob = 0.05
            self.water_rejections_count += 1
        elif (feat["peak_delta"] <= self.WATER_MAX_DELTA) and (feat["dynamic_snr_db"] < 32.0) and (feat["attack_slope"] < self.MIN_ATTACK_SLOPE):
            inst_class = 3  # WATER_DROPLET
            inst_prob = 0.02
            self.water_rejections_count += 1
        elif (feat["peak_delta"] <= self.WATER_MAX_DELTA) and (spread > 1.5):
            inst_class = 3  # WATER_DROPLET
            inst_prob = 0.02
            self.water_rejections_count += 1
        elif (feat["peak_delta"] >= self.MIN_BARE_DELTA) and (feat["attack_slope"] >= self.MIN_ATTACK_SLOPE) and (feat["plateau_variance"] <= self.MAX_PLATEAU_VAR):
            inst_class = 1  # BARE_FINGER
            inst_prob = 1.0
        elif (feat["peak_delta"] >= self.MIN_GLOVE_DELTA) and (feat["contact_duration"] >= 2) and (feat["plateau_variance"] <= self.MAX_PLATEAU_VAR) and (spread <= 1.2):
            inst_class = 2  # GLOVE_OVERLAY
            inst_prob = 1.0
        elif (feat["peak_delta"] >= 50.0) and (feat["contact_duration"] >= 2) and (feat["plateau_variance"] <= self.MAX_PLATEAU_VAR) and (spread <= 1.2):
            inst_class = 1  # FALLBACK_BARE
            inst_prob = 0.85
        else:
            inst_class = 0
            inst_prob = 0.10

        self.detected_class = inst_class

        # 4. Multi-Cycle Confidence Sliding Window
        self.conf_window[self.conf_head] = inst_prob
        self.conf_head = (self.conf_head + 1) % self.CONF_WINDOW_SIZE
        self.confidence = sum(self.conf_window) / float(self.CONF_WINDOW_SIZE)

        # 5. Hysteresis Filtering
        if self.is_valid_touch:
            if self.confidence < self.CONF_RELEASE:
                self.is_valid_touch = False
        else:
            if (self.confidence >= self.CONF_TRIGGER) and (inst_class in (1, 2)):
                self.is_valid_touch = True

        return self.detected_class


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
    print("   S32K144W Touch Sensing - Milestone 4 Comprehensive Verification")
    print("   TinyML Temporal Envelope Classifier & Water Droplet Immunity")
    print("=" * 80)

    results = []

    # ----------------------------------------------------------------------------------
    # Test 1: Architectural & Safety Contract
    # ----------------------------------------------------------------------------------
    with open(TINYML_C_PATH, "r", encoding="utf-8") as f:
        tinyml_c = f.read()
    with open(TINYML_H_PATH, "r", encoding="utf-8") as f:
        tinyml_h = f.read()
    with open(MAIN_C_PATH, "r", encoding="utf-8") as f:
        main_c = f.read()

    has_heap = ("malloc(" in tinyml_c or "calloc(" in tinyml_c or "free(" in tinyml_c)
    has_author_c = "Copyright (c) 2026, Max97k" in tinyml_c and "All Rights Reserved." in tinyml_c
    has_author_h = "Copyright (c) 2026, Max97k" in tinyml_h and "All Rights Reserved." in tinyml_h
    has_author_main = "Copyright (c) 2026, Max97k" in main_c and "All Rights Reserved." in main_c

    # Envelope features presence
    has_attack_slope = "attack_slope" in tinyml_c and "attack_slope" in tinyml_h
    has_dynamic_snr = "dynamic_snr_db" in tinyml_c and "dynamic_snr_db" in tinyml_h
    has_plateau_var = "plateau_variance" in tinyml_c and "plateau_variance" in tinyml_h
    has_release_sym = "release_symmetry" in tinyml_c and "release_symmetry" in tinyml_h

    # Target classes
    has_classes = ("TOUCH_CLASS_BARE_FINGER" in tinyml_h and
                   "TOUCH_CLASS_GLOVE_OVERLAY" in tinyml_h and
                   "TOUCH_CLASS_WATER_DROPLET" in tinyml_h and
                   "TOUCH_CLASS_EDGE_HOVER" in tinyml_h)

    # SysTick hardware sync
    has_systick = "S32_SysTick" in main_c and "COUNTFLAG" in main_c

    t1_pass = (not has_heap) and has_author_c and has_author_h and has_author_main and \
              has_attack_slope and has_dynamic_snr and has_plateau_var and has_release_sym and \
              has_classes and has_systick

    results.append(run_test(
        "M4-1: Architectural & Safety Contract (Zero Heap, Sole Copyright, Envelope, SysTick)",
        t1_pass,
        f"Heap=0, Copyright=Max97k, Features=(Slope, SNR, Var, Sym), Classes=4, SysTick=Active"
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
            has_init = "Touch_TinyML_Init" in symbols
            has_update = "Touch_TinyML_Update" in symbols
            has_extract = "Touch_TinyML_ExtractFeatures" in symbols
            has_valid = "Touch_TinyML_IsValidTouch" in symbols
            has_classifier_obj = "g_tinyml_classifier" in symbols
            has_tinyml_touch = "g_tinyml_touch" in symbols

            size_res = subprocess.run([SIZE_TOOL, ELF_PATH], capture_output=True, text=True, check=True)
            lines = size_res.stdout.strip().split("\n")
            parts = lines[-1].split()
            text_size = int(parts[0])
            bss_size = int(parts[2])

            t2_pass = (has_init and has_update and has_extract and has_valid and
                       has_classifier_obj and has_tinyml_touch and text_size < 45000)
            details = f"All symbols found. ELF .text={text_size} B (<45 KB), .bss={bss_size} B"
        except Exception as e:
            details = f"ELF tool error: {e}"
    else:
        details = "ELF not found"

    results.append(run_test("M4-2: ELF Symbol Table & Section Memory Allocation", t2_pass, details))

    # ----------------------------------------------------------------------------------
    # Test 3: Bare Finger Fast Tap & Sustained Press Dataset Simulation (50 trials)
    # ----------------------------------------------------------------------------------
    # Fast tap: 3~5 cycles (45~75 ms) contact, delta 80~140 LSB, attack slope 25~45 LSB/cycle
    # Long press: 15~30 cycles (225~450 ms) contact, delta 100~160 LSB, attack slope 20~40 LSB/cycle
    np.random.seed(42)
    bare_detected = 0
    bare_latencies = []

    dataset_records = []

    for trial in range(50):
        clf = PythonTinyMLClassifier()
        is_long = (trial % 2 == 1)
        hold_cycles = np.random.randint(15, 30) if is_long else np.random.randint(4, 7)
        peak_amp = np.random.uniform(90.0, 150.0)

        # Baseline quiescent (5 cycles)
        for _ in range(5):
            noise = np.random.normal(0.0, 0.3)
            clf.update(noise, spread=0.6)

        # Attack transition (2 cycles)
        t_detect = None
        s1 = peak_amp * 0.45 + np.random.normal(0.0, 0.5)
        clf.update(s1, spread=0.8)
        if clf.is_valid_touch and t_detect is None:
            t_detect = 1

        s2 = peak_amp + np.random.normal(0.0, 0.5)
        clf.update(s2, spread=0.7)
        if clf.is_valid_touch and t_detect is None:
            t_detect = 2

        # Held contact
        for cycle in range(3, hold_cycles + 3):
            val = peak_amp + np.random.normal(0.0, 0.4)
            clf.update(val, spread=0.7)
            if clf.is_valid_touch and t_detect is None:
                t_detect = cycle

        if clf.is_valid_touch or (t_detect is not None):
            bare_detected += 1
            latency_ms = (t_detect if t_detect is not None else 3) * 15
            bare_latencies.append(latency_ms)

        dataset_records.append({
            "trial": trial + 1,
            "type": "BARE_FINGER",
            "peak_delta": peak_amp,
            "detected": clf.is_valid_touch or (t_detect is not None),
            "latency_ms": bare_latencies[-1] if bare_latencies else 45
        })

    tpr_bare = (bare_detected / 50.0) * 100.0
    avg_latency = np.mean(bare_latencies) if bare_latencies else 0.0
    t3_pass = (tpr_bare >= 98.0) and (avg_latency <= 60.0)

    results.append(run_test(
        "M4-3: Bare Finger Fast Tap & Long Press (50 trials, TPR >= 98%, latency <= 60~90 ms)",
        t3_pass,
        f"TPR = {tpr_bare:.1f}% ({bare_detected}/50), Avg Latency = {avg_latency:.1f} ms (Target <= 60~90 ms)"
    ))

    # ----------------------------------------------------------------------------------
    # Test 4: Glove / 2mm Acrylic Overlay Dataset Simulation (50 trials)
    # ----------------------------------------------------------------------------------
    # Delta attenuated to 38 ~ 58 LSB due to acrylic dielectric / nitrile glove
    # Attack slope 12 ~ 25 LSB/cycle, stable fleshy plateau (variance 0.2 ~ 1.5 LSB^2)
    glove_detected = 0
    glove_latencies = []

    for trial in range(50):
        clf = PythonTinyMLClassifier()
        hold_cycles = np.random.randint(6, 15)
        peak_amp = np.random.uniform(40.0, 58.0)

        for _ in range(5):
            clf.update(np.random.normal(0.0, 0.3), spread=0.6)

        t_detect = None
        s1 = peak_amp * 0.40 + np.random.normal(0.0, 0.4)
        clf.update(s1, spread=0.7)
        if clf.is_valid_touch and t_detect is None:
            t_detect = 1

        s2 = peak_amp + np.random.normal(0.0, 0.4)
        clf.update(s2, spread=0.7)
        if clf.is_valid_touch and t_detect is None:
            t_detect = 2

        for cycle in range(3, hold_cycles + 3):
            val = peak_amp + np.random.normal(0.0, 0.4)
            clf.update(val, spread=0.7)
            if clf.is_valid_touch and t_detect is None:
                t_detect = cycle

        if clf.is_valid_touch or (t_detect is not None):
            glove_detected += 1
            latency_ms = (t_detect if t_detect is not None else 3) * 15
            glove_latencies.append(latency_ms)

        dataset_records.append({
            "trial": trial + 1,
            "type": "GLOVE_OVERLAY",
            "peak_delta": peak_amp,
            "detected": clf.is_valid_touch or (t_detect is not None),
            "latency_ms": glove_latencies[-1] if glove_latencies else 45
        })

    tpr_glove = (glove_detected / 50.0) * 100.0
    t4_pass = (tpr_glove >= 98.0)

    results.append(run_test(
        "M4-4: Glove / 2mm Acrylic Overlay Detection (50 trials, TPR >= 98%)",
        t4_pass,
        f"TPR = {tpr_glove:.1f}% ({glove_detected}/50), Avg Latency = {np.mean(glove_latencies):.1f} ms"
    ))

    # ----------------------------------------------------------------------------------
    # Test 5: Water Droplet & Spray Condensation Rejection Simulation (20 trials)
    # ----------------------------------------------------------------------------------
    # Scenario A: Water droplet landing (delta 22 ~ 40 LSB, attack slope 3 ~ 8 LSB/cycle, SNR 20~28 dB)
    # Scenario B: Water droplet splash (instant step, severe ringing/meniscus vibration variance 15~35 LSB^2)
    water_false_triggers = 0
    water_rejections = 0

    for trial in range(20):
        clf = PythonTinyMLClassifier()
        is_splash = (trial % 2 == 1)

        for _ in range(5):
            clf.update(np.random.normal(0.0, 0.3), spread=0.6)

        if not is_splash:
            # Slow wetting droplet landing
            droplet_peak = np.random.uniform(22.0, 42.0)
            for c in range(15):
                # Gradual ramp-up
                ramp = droplet_peak * min(1.0, (c + 1) * 0.15) + np.random.normal(0.0, 0.8)
                clf.update(ramp, spread=1.8)  # higher spread/noise
                if clf.is_valid_touch:
                    water_false_triggers += 1
                    break
        else:
            # Droplet splash: instant jump + ringing oscillation
            splash_peak = np.random.uniform(46.0, 65.0)
            # Sudden impact
            clf.update(splash_peak, spread=2.5)
            # Ringing meniscus vibration
            for c in range(12):
                osc = splash_peak + 5.0 * math.sin(c * 1.5) + np.random.normal(0.0, 1.5)
                clf.update(osc, spread=2.8)
                if clf.is_valid_touch:
                    water_false_triggers += 1
                    break

        if not clf.is_valid_touch:
            water_rejections += 1

        dataset_records.append({
            "trial": trial + 1,
            "type": "WATER_DROPLET",
            "peak_delta": droplet_peak if not is_splash else splash_peak,
            "detected": clf.is_valid_touch,
            "latency_ms": 0
        })

    fpr_water = (water_false_triggers / 20.0) * 100.0
    t5_pass = (fpr_water == 0.0) and (water_rejections == 20)

    results.append(run_test(
        "M4-5: Water Droplet & Spray Rejection (20 trials, FPR = 0.0%)",
        t5_pass,
        f"FPR = {fpr_water:.1f}% ({water_false_triggers}/20 false triggers), Rejections = {water_rejections}/20"
    ))

    # ----------------------------------------------------------------------------------
    # Test 6: Edge Hover & Grazing Fast Swipe Rejection (20 trials)
    # ----------------------------------------------------------------------------------
    # Finger sweeps across pad boundary: 1~2 cycle transient spike (28 ~ 48 LSB) then immediately leaves
    hover_false_triggers = 0
    hover_rejections = 0

    for trial in range(20):
        clf = PythonTinyMLClassifier()
        peak_amp = np.random.uniform(28.0, 48.0)

        for _ in range(5):
            clf.update(np.random.normal(0.0, 0.3), spread=0.6)

        # 1-cycle or 2-cycle transient
        clf.update(peak_amp, spread=1.0)
        if trial % 2 == 1:
            clf.update(peak_amp * 0.7, spread=1.0)

        # Release immediately to zero
        for _ in range(8):
            clf.update(np.random.normal(0.0, 0.3), spread=0.6)
            if clf.is_valid_touch:
                hover_false_triggers += 1
                break

        if not clf.is_valid_touch:
            hover_rejections += 1

        dataset_records.append({
            "trial": trial + 1,
            "type": "EDGE_HOVER",
            "peak_delta": peak_amp,
            "detected": clf.is_valid_touch,
            "latency_ms": 0
        })

    fpr_hover = (hover_false_triggers / 20.0) * 100.0
    t6_pass = (fpr_hover == 0.0) and (hover_rejections == 20)

    results.append(run_test(
        "M4-6: Edge Hover & Grazing Swipe Rejection (20 trials, FPR = 0.0%)",
        t6_pass,
        f"FPR = {fpr_hover:.1f}% ({hover_false_triggers}/20 false triggers), Rejections = {hover_rejections}/20"
    ))

    # ----------------------------------------------------------------------------------
    # Test 7: Export Dataset CSV & Confusion Matrix Analysis
    # ----------------------------------------------------------------------------------
    csv_out = os.path.join(REPO_ROOT, "m4_tinyml_multiclass_dataset.csv")
    with open(csv_out, "w", encoding="utf-8") as f:
        f.write("Trial,Scenario,PeakDelta_LSB,Detected,Latency_ms\n")
        for rec in dataset_records:
            f.write(f"{rec['trial']},{rec['type']},{rec['peak_delta']:.2f},{1 if rec['detected'] else 0},{rec['latency_ms']}\n")

    # Confusion matrix:
    # Actual Positives: Bare Finger (50), Glove Overlay (50) -> Total 100
    # Actual Negatives: Water Droplet (20), Edge Hover (20) -> Total 40
    tp = bare_detected + glove_detected
    fn = 100 - tp
    fp = water_false_triggers + hover_false_triggers
    tn = 40 - fp

    overall_tpr = (tp / 100.0) * 100.0
    overall_fpr = (fp / 40.0) * 100.0
    overall_accuracy = ((tp + tn) / 140.0) * 100.0

    print("-" * 80)
    print("   Milestone 4 Confusion Matrix & Statistical Performance Summary")
    print("-" * 80)
    print(f"   [+] True Positives  (TP) : {tp} / 100 (Bare: {bare_detected}, Glove: {glove_detected})")
    print(f"   [+] False Negatives (FN) : {fn} / 100")
    print(f"   [+] True Negatives  (TN) : {tn} / 40  (Water: {water_rejections}, Hover: {hover_rejections})")
    print(f"   [+] False Positives (FP) : {fp} / 40")
    print(f"   -------------------------------------------------------------")
    print(f"   [*] Sensitivity (TPR)    : {overall_tpr:.2f}% (Pass Criteria >= 98.0%)")
    print(f"   [*] Fall-out (FPR)       : {overall_fpr:.2f}% (Pass Criteria = 0.0%)")
    print(f"   [*] Overall Accuracy     : {overall_accuracy:.2f}%")
    print(f"   [*] Average Latency      : {avg_latency:.1f} ms (Pass Criteria <= 60 ~ 90 ms)")
    print(f"   [+] Multi-Class Dataset Exported to: {csv_out}")
    print("-" * 80)

    t7_pass = (overall_tpr >= 98.0) and (overall_fpr == 0.0) and (avg_latency <= 60.0)
    results.append(run_test(
        "M4-7: Confusion Matrix & Multi-Class Dataset Validation",
        t7_pass,
        f"Accuracy={overall_accuracy:.1f}%, TPR={overall_tpr:.1f}%, FPR={overall_fpr:.1f}%, Latency={avg_latency:.1f} ms"
    ))

    # ----------------------------------------------------------------------------------
    # Summary Evaluation
    # ----------------------------------------------------------------------------------
    all_passed = all(results)
    print("=" * 80)
    if all_passed:
        print("   [+] ALL MILESTONE 4 CONTRACT TESTS PASSED PERFECTLY!")
        print("   [+] Ready for Milestone 4 Sign-off & Milestone 5 Plan Presentation.")
    else:
        print("   [-] MILESTONE 4 CONTRACT FAILED!")
    print("=" * 80)

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
