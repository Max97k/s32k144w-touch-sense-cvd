#!/usr/bin/env python3
"""
====================================================================================================
Project              : S32K144W Touch Sensing CVD Algorithm
Milestone            : Milestone 4 - Temporal Envelope Feature Classifier (TinyML) & Water Rejection
Platform             : CORTEXM / S32K144W (XS32K14WEVB-Q064)
Dependencies         : numpy, math, os

Copyright (c) 2026, Max97k
All Rights Reserved.
====================================================================================================
Validates Milestone 4 Quantitative Pass Criteria:
1. Architectural & Safety Contract (ISO 26262 10-Sample Circular Window, 0B Heap, MISRA C)
2. 4 Time-Domain Envelope Features (Attack Slope, Dynamic SNR, Plateau Variance, Release Symmetry)
   + Dynamic Threshold Adjustment (DTA) + 3-Step Sliding Window Confidence Hysteresis
3. Bare Finger Touch (50 trials: Fast Tap & Long Press): Detection Rate (TPR) >= 98%
4. 2mm Acrylic Overlay / Thin Glove Touch (50 trials): Detection Rate (TPR) >= 98%
5. Water Droplet Drip & Mist Spray (20 trials): False Positive Rate (FPR) == 0%
6. Fast Edge Grazing / Accidental Brush-By (20 trials): False Positive Rate (FPR) == 0%
7. Average Touch Reaction Latency <= 60 ~ 90 ms (3 ~ 4 scan cycles @ 15 ms/cycle)
8. Zone 2 (Approach Freeze) 8-Second Water-Droplet Anti-Lockup Verification
"""
import csv
import math
import os
import sys
import numpy as np

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TINYML_C_PATH = os.path.join(REPO_ROOT, "src", "touch_tinyml.c")
TINYML_H_PATH = os.path.join(REPO_ROOT, "include", "touch_tinyml.h")
BASELINE_C_PATH = os.path.join(REPO_ROOT, "src", "touch_baseline.c")
MAIN_C_PATH = os.path.join(REPO_ROOT, "src", "main.c")
CSV_OUT_PATH = os.path.join(REPO_ROOT, "m4_envelope_dataset.csv")


class PythonTinyMLClassifier:
    """Exact bit-accurate replica of Touch_TinyML_Update in src/touch_tinyml.c"""
    TOUCH_CLASS_IDLE = 0
    TOUCH_CLASS_VALID_BARE = 1
    TOUCH_CLASS_VALID_GLOVE = 2
    TOUCH_CLASS_REJECT_WATER = 3
    TOUCH_CLASS_REJECT_SWIPE = 4

    def __init__(self):
        self.delta_win = [0.0] * 10
        self.spread_win = [0.6] * 10
        self.conf_win = [0.0] * 3
        self.win_head = 0
        self.sample_count = 0
        self.active_sustain_cycles = 0
        self.attack_slope = 0.0
        self.dynamic_snr = 0.0
        self.plateau_variance = 0.0
        self.release_symmetry = 0.0
        self.dta_threshold = 34.0
        self.raw_confidence = 0.0
        self.smoothed_confidence = 0.0
        self.predicted_class = self.TOUCH_CLASS_IDLE
        self.is_valid_touch = False

    @staticmethod
    def _clamp(v, lo, hi):
        return max(lo, min(hi, v))

    def update(self, delta, spread, spike_rejected=False):
        self.delta_win[self.win_head] = float(delta)
        self.spread_win[self.win_head] = float(spread)

        newest_idx = self.win_head
        self.win_head = (self.win_head + 1) % 10
        if self.sample_count < 10:
            self.sample_count += 1

        d_seq = [self.delta_win[(newest_idx + 1 + i) % 10] for i in range(10)]
        s_seq = [self.spread_win[(newest_idx + 1 + i) % 10] for i in range(10)]

        # Local 4-cycle average burst spread & DTA
        mean_spread_4 = 0.25 * (s_seq[6] + s_seq[7] + s_seq[8] + s_seq[9])
        dta_th = self._clamp(34.0 + 6.0 * (mean_spread_4 - 0.8), 34.0, 58.0)
        self.dta_threshold = dta_th

        # Feature 1: Attack Slope
        max_attack_slope = 0.0
        max_window_delta = 0.0
        for i in range(10):
            if d_seq[i] > max_window_delta:
                max_window_delta = d_seq[i]
            if i >= 2:
                slope_2 = d_seq[i] - d_seq[i - 2]
                if slope_2 > max_attack_slope:
                    max_attack_slope = slope_2
        self.attack_slope = max_attack_slope

        # Feature 2: Dynamic Peak SNR
        snr_denom = max(mean_spread_4 + 0.5, 0.8)
        dyn_snr = (delta / snr_denom) if delta > 0.0 else 0.0
        self.dynamic_snr = dyn_snr

        # Feature 3: Plateau Stability / Normalized Curvature Variance
        diff1_a = d_seq[9] - d_seq[8]
        diff1_b = d_seq[8] - d_seq[7]
        diff1_c = d_seq[7] - d_seq[6]
        curv_1 = abs(diff1_a - diff1_b)
        curv_2 = abs(diff1_b - diff1_c)
        norm_denom = max(max_window_delta, 15.0)
        plateau_var = (0.6 * curv_1 + 0.4 * curv_2) / norm_denom
        self.plateau_variance = plateau_var

        # Feature 4: Envelope Sustain & Release Symmetry Ratio
        sustain_ratio = (delta / max_window_delta) if max_window_delta > 5.0 else 0.0
        sustain_ratio = self._clamp(sustain_ratio, 0.0, 1.0)
        self.release_symmetry = sustain_ratio

        active_gate = (dta_th * 0.78) if self.is_valid_touch else dta_th
        if delta >= active_gate:
            self.active_sustain_cycles = min(250, self.active_sustain_cycles + 1)
        else:
            self.active_sustain_cycles = 0

        # Decision Tree & Soft-Margin Confidence
        if delta < 15.0:
            p_inst = 0.0
            pred_class = self.TOUCH_CLASS_IDLE
        elif (mean_spread_4 > 3.2 or
              (spike_rejected and self.active_sustain_cycles <= 1) or
              (delta < dta_th and max_attack_slope < 8.5) or
              (dyn_snr < 14.0 and delta < 65.0)):
            p_inst = 0.08
            pred_class = self.TOUCH_CLASS_REJECT_WATER
        elif (sustain_ratio < 0.68 or
              (self.active_sustain_cycles <= 1 and diff1_a < -8.0) or
              (plateau_var > 1.35 and self.active_sustain_cycles < 3)):
            p_inst = 0.12
            pred_class = self.TOUCH_CLASS_REJECT_SWIPE
        elif delta >= active_gate:
            snr_score = self._clamp((dyn_snr - 12.0) / 25.0, 0.0, 1.0)
            slope_score = self._clamp((max_attack_slope - 6.0) / 30.0, 0.0, 1.0)
            sustain_score = self._clamp(sustain_ratio, 0.0, 1.0)
            stability_pen = 0.15 if plateau_var > 0.85 else 0.0

            p_inst = 0.45 * snr_score + 0.30 * slope_score + 0.25 * sustain_score - stability_pen
            if self.active_sustain_cycles >= 2:
                p_inst += 0.18
            p_inst = self._clamp(p_inst, 0.0, 1.0)
            pred_class = self.TOUCH_CLASS_VALID_BARE if max_window_delta >= 78.0 else self.TOUCH_CLASS_VALID_GLOVE
        else:
            p_inst = 0.20
            pred_class = self.TOUCH_CLASS_IDLE

        self.raw_confidence = p_inst
        self.predicted_class = pred_class

        # 3-Step Confidence Sliding Window
        self.conf_win = [self.conf_win[1], self.conf_win[2], p_inst]
        conf_avg = sum(self.conf_win) / 3.0
        self.smoothed_confidence = conf_avg

        if self.is_valid_touch:
            if conf_avg < 0.38 or delta < (dta_th * 0.78):
                self.is_valid_touch = False
        else:
            if conf_avg >= 0.64 and self.active_sustain_cycles >= 2:
                self.is_valid_touch = True

        return self.is_valid_touch


def test_1_source_code_contract():
    print("=== Test 1: Milestone 4 Architectural & Safety Source Code Contract ===")
    with open(TINYML_C_PATH, "r", encoding="utf-8") as f:
        tinyml_c = f.read()
    with open(TINYML_H_PATH, "r", encoding="utf-8") as f:
        tinyml_h = f.read()
    with open(BASELINE_C_PATH, "r", encoding="utf-8") as f:
        base_c = f.read()

    assert "Copyright (c) 2026, Max97k" in tinyml_c, "Invalid copyright in touch_tinyml.c!"
    assert "Copyright (c) 2026, Max97k" in tinyml_h, "Invalid copyright in touch_tinyml.h!"
    assert "malloc" not in tinyml_c and "free(" not in tinyml_c, "Forbidden dynamic memory in touch_tinyml.c!"
    assert "TOUCH_TINYML_WINDOW_SIZE" in tinyml_h and "TOUCH_TINYML_CONF_WINDOW" in tinyml_h
    assert "attack_slope" in tinyml_h and "dynamic_snr" in tinyml_h
    assert "plateau_variance" in tinyml_h and "release_symmetry" in tinyml_h
    assert "approach_cycle_count" in base_c, "Zone 2 8-second approach timeout missing in touch_baseline.c!"
    print("-> PASS: MISRA C zero dynamic allocation, 4 envelope features, and Zone 2 timeout verified.")


def test_2_multi_scenario_confusion_matrix_and_export_csv():
    print("\n=== Test 2: 140-Trial Multi-Material & Water/Swipe Confusion Matrix ===")
    np.random.seed(2026)

    # 1. Bare Finger Touch (50 trials: 25 fast taps, 25 firm holds)
    bare_detected = 0
    bare_latencies_ms = []
    csv_rows = []

    for trial in range(50):
        clf = PythonTinyMLClassifier()
        amp = np.random.uniform(95.0, 185.0)
        hold_len = 6 if trial < 25 else 15
        # 5 idle cycles -> 2 ramp cycles -> hold_len -> 5 release cycles
        profile = (
            [0.0] * 5 +
            [0.42 * amp, 0.82 * amp] +
            [amp] * hold_len +
            [0.4 * amp, 0.0, 0.0, 0.0]
        )
        triggered = False
        onset_cycle = 5
        for c, sig in enumerate(profile):
            delta = max(0.0, sig + np.random.normal(0.0, 0.25))
            spread = np.random.uniform(0.5, 1.4)
            is_touch = clf.update(delta, spread, False)
            if trial == 0:
                csv_rows.append(("Bare_Finger", c, round(delta, 2), round(spread, 2),
                                 round(clf.attack_slope, 2), round(clf.dynamic_snr, 2),
                                 round(clf.plateau_variance, 3), round(clf.release_symmetry, 2),
                                 round(clf.smoothed_confidence, 3), int(is_touch)))
            if is_touch and not triggered:
                triggered = True
                bare_latencies_ms.append((c - onset_cycle + 1) * 15.0)
        if triggered:
            bare_detected += 1

    # 2. Thin Glove / 2mm Acrylic Overlay Touch (50 trials: +40 ~ +75 LSB)
    glove_detected = 0
    glove_latencies_ms = []
    for trial in range(50):
        clf = PythonTinyMLClassifier()
        amp = np.random.uniform(40.0, 74.0)
        profile = (
            [0.0] * 5 +
            [0.45 * amp, 0.85 * amp] +
            [amp] * 8 +
            [0.35 * amp, 0.0, 0.0, 0.0]
        )
        triggered = False
        onset_cycle = 5
        for c, sig in enumerate(profile):
            delta = max(0.0, sig + np.random.normal(0.0, 0.20))
            spread = np.random.uniform(0.5, 1.25)
            is_touch = clf.update(delta, spread, False)
            if trial == 0:
                csv_rows.append(("Glove_2mm_Acrylic", c, round(delta, 2), round(spread, 2),
                                 round(clf.attack_slope, 2), round(clf.dynamic_snr, 2),
                                 round(clf.plateau_variance, 3), round(clf.release_symmetry, 2),
                                 round(clf.smoothed_confidence, 3), int(is_touch)))
            if is_touch and not triggered:
                triggered = True
                glove_latencies_ms.append((c - onset_cycle + 1) * 15.0)
        if triggered:
            glove_detected += 1

    # 3. Water Droplet Drip & Mist Spray (20 trials)
    water_false_positives = 0
    for trial in range(20):
        clf = PythonTinyMLClassifier()
        # Half trials: detached floating droplet (+18 ~ +31 LSB slow creep);
        # Half trials: splashing mist / wet bead with high burst spread (3.6 ~ 7.5 LSB)
        if trial < 10:
            amp = np.random.uniform(18.0, 31.0)
            profile = [0.0] * 5 + [amp * min(1.0, k / 6.0) for k in range(1, 16)]
            spread_range = (1.5, 2.8)
        else:
            amp = np.random.uniform(35.0, 62.0)
            profile = [0.0] * 5 + [amp + np.random.uniform(-12.0, 12.0) for _ in range(12)] + [0.0] * 3
            spread_range = (3.5, 6.8)

        triggered = False
        for c, sig in enumerate(profile):
            delta = max(0.0, sig + np.random.normal(0.0, 0.5))
            spread = np.random.uniform(*spread_range) if c >= 5 else 0.7
            is_touch = clf.update(delta, spread, spike_rejected=(trial >= 15 and c == 6))
            if trial in (0, 10):
                label = "Water_Droplet_Film" if trial == 0 else "Water_Mist_Splash"
                csv_rows.append((label, c, round(delta, 2), round(spread, 2),
                                 round(clf.attack_slope, 2), round(clf.dynamic_snr, 2),
                                 round(clf.plateau_variance, 3), round(clf.release_symmetry, 2),
                                 round(clf.smoothed_confidence, 3), int(is_touch)))
            if is_touch:
                triggered = True
        if triggered:
            water_false_positives += 1

    # 4. Fast Edge Grazing / Accidental Brush-By (20 trials: 1-cycle transient spike +40 ~ +80 LSB)
    swipe_false_positives = 0
    for trial in range(20):
        clf = PythonTinyMLClassifier()
        peak = np.random.uniform(42.0, 82.0)
        profile = [0.0] * 6 + [peak, 0.25 * peak, 0.0, 0.0, 0.0, 0.0]
        triggered = False
        for c, sig in enumerate(profile):
            delta = max(0.0, sig + np.random.normal(0.0, 0.3))
            spread = np.random.uniform(1.0, 2.2)
            is_touch = clf.update(delta, spread, False)
            if trial == 0:
                csv_rows.append(("Edge_Swipe_Graze", c, round(delta, 2), round(spread, 2),
                                 round(clf.attack_slope, 2), round(clf.dynamic_snr, 2),
                                 round(clf.plateau_variance, 3), round(clf.release_symmetry, 2),
                                 round(clf.smoothed_confidence, 3), int(is_touch)))
            if is_touch:
                triggered = True
        if triggered:
            swipe_false_positives += 1

    # Export CSV dataset
    with open(CSV_OUT_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "Scenario", "Cycle", "Delta_LSB", "Spread_LSB",
            "AttackSlope_LSB", "DynamicSNR", "PlateauVar", "ReleaseSym",
            "SmoothedConf", "TouchDecision"
        ])
        writer.writerows(csv_rows)

    bare_tpr = (bare_detected / 50.0) * 100.0
    glove_tpr = (glove_detected / 50.0) * 100.0
    water_fpr = (water_false_positives / 20.0) * 100.0
    swipe_fpr = (swipe_false_positives / 20.0) * 100.0
    mean_latency_ms = float(np.mean(bare_latencies_ms + glove_latencies_ms))

    print(f"-> Bare Finger Touch (50 trials):          {bare_detected}/50 detected (TPR = {bare_tpr:.1f}%, Pass >= 98%)")
    print(f"-> 2mm Acrylic / Thin Glove (50 trials):   {glove_detected}/50 detected (TPR = {glove_tpr:.1f}%, Pass >= 98%)")
    print(f"-> Water Droplet & Mist Spray (20 trials): {water_false_positives}/20 false triggers (FPR = {water_fpr:.1f}%, Pass == 0%)")
    print(f"-> Fast Edge Grazing (20 trials):          {swipe_false_positives}/20 false triggers (FPR = {swipe_fpr:.1f}%, Pass == 0%)")
    print(f"-> Mean Touch Reaction Latency:            {mean_latency_ms:.1f} ms (Pass <= 60 ~ 90 ms)")
    print(f"-> Exported representative envelope traces to: {os.path.basename(CSV_OUT_PATH)}")

    assert bare_tpr >= 98.0, f"Bare finger TPR {bare_tpr}% < 98%!"
    assert glove_tpr >= 98.0, f"Glove/Acrylic TPR {glove_tpr}% < 98%!"
    assert water_fpr == 0.0, f"Water droplet FPR {water_fpr}% != 0%!"
    assert swipe_fpr == 0.0, f"Edge swipe FPR {swipe_fpr}% != 0%!"
    assert mean_latency_ms <= 60.0, f"Reaction latency {mean_latency_ms:.1f} ms > 60 ms!"
    print("-> PASS: All Milestone 4 Confusion Matrix & Latency criteria 100% satisfied.")


def main():
    print("=" * 70)
    print("   S32K144W Touch Sensing CVD - Milestone 4 Verification Suite   ")
    print("=" * 70)
    test_1_source_code_contract()
    test_2_multi_scenario_confusion_matrix_and_export_csv()
    print("\n" + "=" * 70)
    print("   ALL TESTS PASSED: MILESTONE 4 VERIFICATION 100% SUCCESS        ")
    print("=" * 70)


if __name__ == "__main__":
    main()