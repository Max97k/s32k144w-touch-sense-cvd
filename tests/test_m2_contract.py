#!/usr/bin/env python3
"""
====================================================================================================
Project              : S32K144W Touch Sensing CVD Algorithm
Milestone            : Milestone 2 - Spread Spectrum, Multi-Frequency Hopping & Adaptive Kalman Filter
Platform             : CORTEXM / S32K144W (XS32K14WEVB-Q064)
Dependencies         : numpy, math, subprocess, re, os

Copyright (c) 2026, Max97k
All Rights Reserved.
====================================================================================================
Validates Milestone 2 Quantitative Pass Criteria:
1. Architectural & Safety Contract (ISO 26262 Deterministic O(N^2) Sort, Zero Heap, MISRA C)
2. ELF Binary Symbol Table & Section Memory Allocation
3. Baseline Noise Reduction: 500-cycle quiet baseline sigma < 1.5 LSB (target < 0.3 LSB)
4. Signal-to-Noise Ratio (SNR): Touch shift / sigma_noise >= 28 dB
5. EFT / ESD +800 LSB Impulse Stress Immunity: Peak x_est deviation < 3 LSB
6. Fast Tap Dynamic Response: Step tracking latency <= 1 scan cycle
"""
import math
import os
import re
import subprocess
import sys
import numpy as np

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ELF_PATH = os.path.join(REPO_ROOT, "Debug_FLASH", "s32k144w-touch-sense-cvd.elf")
FILTER_C_PATH = os.path.join(REPO_ROOT, "src", "touch_filter.c")
FILTER_H_PATH = os.path.join(REPO_ROOT, "include", "touch_filter.h")
MAIN_C_PATH = os.path.join(REPO_ROOT, "src", "main.c")

NM_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-nm.exe"
SIZE_TOOL = r"C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin\arm-none-eabi-size.exe"


class PythonKalmanFilter:
    """Exact software replica of Touch_Kalman_Update in src/touch_filter.c"""
    def __init__(self, initial_val, q_proc=0.35, r_base=1.0, alpha=0.75):
        self.x_est = float(initial_val)
        self.p_est = 1.0
        self.q_proc = float(q_proc)
        self.r_base = float(r_base)
        self.alpha = float(alpha)
        self.k_gain = 0.5
        self.spread = 0.0

    def update(self, z_meas, spread):
        # 1. Time Update (Predict)
        x_pred = self.x_est
        p_pred = self.p_est + self.q_proc

        # 2. Adaptive Measurement Noise Variance
        r_k = self.r_base + (self.alpha * (spread * spread))

        # 3. Measurement Update (Correct)
        denom = p_pred + r_k
        k_k = p_pred / denom if denom > 1e-5 else 0.5
        k_k = max(0.01, min(0.99, k_k))

        self.x_est = x_pred + k_k * (z_meas - x_pred)
        self.p_est = max(0.001, (1.0 - k_k) * p_pred)
        self.k_gain = k_k
        self.spread = spread
        return self.x_est


def alpha_trimmed_mean(burst_samples):
    """Exact software replica of Touch_AlphaTrimmedMean in src/touch_filter.c"""
    sorted_s = sorted(burst_samples)
    # N = 9: discard indices 0, 1 and 7, 8; keep 2, 3, 4, 5, 6
    trimmed = sorted_s[2:7]
    z_meas = float(sum(trimmed)) / len(trimmed)
    spread = float(sorted_s[6] - sorted_s[2])
    return z_meas, spread


def test_1_source_code_contract():
    print("=== Test 1: Architectural & Safety Source Code Contract ===")
    with open(FILTER_C_PATH, "r", encoding="utf-8") as f:
        filter_c = f.read()
    with open(FILTER_H_PATH, "r", encoding="utf-8") as f:
        filter_h = f.read()
    with open(MAIN_C_PATH, "r", encoding="utf-8") as f:
        main_c = f.read()

    # 1. Sole Copyright Ownership
    assert "Copyright (c) 2026, Max97k" in filter_c, "Invalid copyright in touch_filter.c!"
    assert "Copyright (c) 2026, Max97k" in filter_h, "Invalid copyright in touch_filter.h!"
    assert "NXP / Max97k" not in filter_c and "NXP / Max97k" not in filter_h, "Legacy hybrid copyright found!"
    print("-> PASS: Sole copyright ownership confirmed ('Copyright (c) 2026, Max97k').")

    # 2. MISRA C Zero Dynamic Allocation
    assert "malloc" not in filter_c and "free(" not in filter_c, "Forbidden dynamic memory allocation found!"
    print("-> PASS: MISRA C zero dynamic memory allocation confirmed (0B heap).")

    # 3. 16-bit Galois LFSR Polynomial 0xB400 & All-zero handling
    assert "0xB400U" in filter_h or "0xB400" in filter_c, "LFSR polynomial 0xB400 missing!"
    assert "TOUCH_LFSR_DEFAULT_SEED" in filter_c, "LFSR all-zero forbidden reset missing!"
    print("-> PASS: 16-bit Galois LFSR PRNG (poly 0xB400) verified.")

    # 4. Coprime multi-frequency delay loops (15, 23, 31)
    assert "15U" in filter_h and "23U" in filter_h and "31U" in filter_h, "Coprime delays {15, 23, 31} missing!"
    print("-> PASS: Coprime frequency hopping delay constants {15, 23, 31} verified.")

    # 5. Deterministic Bounded Insertion Sort (O(N^2))
    assert "sorted[j] > key" in filter_c, "Deterministic insertion sort missing!"
    print("-> PASS: Deterministic bounded insertion sort verified.")

    # 6. Adaptive Kalman filter dynamic covariance R_k = R_base + alpha * spread^2
    assert "r_k = kf->r_base + (kf->alpha * (spread * spread));" in filter_c, "Adaptive R_k mapping missing!"
    assert "k_k < 0.01f" in filter_c and "k_k > 0.99f" in filter_c, "Kalman gain bounding missing!"
    print("-> PASS: 1D Adaptive Scalar Kalman Filter formulation & gain clamping verified.")


def test_2_elf_symbol_table():
    print("\n=== Test 2: ELF Symbol Table & Section Memory Allocation ===")
    assert os.path.exists(ELF_PATH), f"Target ELF not found at {ELF_PATH}"

    cmd = [NM_TOOL, "-n", ELF_PATH]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    nm_output = proc.stdout

    m2_symbols = [
        "Touch_LFSR_Next",
        "Touch_AcquireBurstMFP",
        "Touch_AlphaTrimmedMean",
        "Touch_Kalman_Init",
        "Touch_Kalman_Update",
        "g_kalman_filter",
        "g_m2_sample",
        "g_lfsr_state"
    ]

    for sym in m2_symbols:
        assert re.search(rf"\s{sym}$", nm_output, re.MULTILINE), f"Symbol {sym} missing from ELF!"
        print(f"-> PASS: Verified symbol '{sym}' linked in ELF.")

    size_cmd = [SIZE_TOOL, "-B", ELF_PATH]
    size_proc = subprocess.run(size_cmd, capture_output=True, text=True, check=True)
    print(size_proc.stdout.strip())


def test_3_quiet_baseline_noise_reduction():
    print("\n=== Test 3: 500-Cycle Quiet Baseline Noise & SNR Verification ===")
    np.random.seed(42)
    baseline_true = -334.14
    sigma_raw = 0.586  # From M1 hardware empirical measurement

    kf = PythonKalmanFilter(initial_val=baseline_true, q_proc=0.35, r_base=1.0, alpha=0.75)

    raw_samples = []
    z_meas_samples = []
    x_est_samples = []
    spread_samples = []

    for _ in range(500):
        # Generate 9-sample burst with M1 Gaussian physical noise
        burst = np.random.normal(baseline_true, sigma_raw, 9)
        raw_samples.append(burst[4])

        z_m, spr = alpha_trimmed_mean(burst)
        x_e = kf.update(z_m, spr)

        z_meas_samples.append(z_m)
        x_est_samples.append(x_e)
        spread_samples.append(spr)

    sigma_raw_est = float(np.std(raw_samples))
    sigma_z_meas = float(np.std(z_meas_samples))
    sigma_x_est = float(np.std(x_est_samples))
    mean_spread = float(np.mean(spread_samples))

    print(f"-> Raw Single-Shot Sigma:    {sigma_raw_est:.4f} LSB")
    print(f"-> Alpha-Trimmed Mean Sigma: {sigma_z_meas:.4f} LSB (Noise suppression: {sigma_raw_est / sigma_z_meas:.2f}x)")
    print(f"-> Adaptive Kalman Sigma:    {sigma_x_est:.4f} LSB (Total suppression: {sigma_raw_est / sigma_x_est:.2f}x)")
    print(f"-> Mean High-Freq Spread:    {mean_spread:.4f} LSB")

    # M2 Pass Criterion: Filtered sigma < 1.5 LSB
    assert sigma_x_est < 1.5, f"Filtered noise sigma {sigma_x_est:.4f} exceeds 1.5 LSB!"
    print(f"-> PASS: Filtered baseline noise sigma ({sigma_x_est:.4f} LSB) << 1.5 LSB.")

    # Touch SNR Calculation
    delta_touch_mean = 195.0  # From M1 hardware 2x differential touch shift
    snr_db = 20.0 * math.log10(delta_touch_mean / sigma_x_est)
    print(f"-> Touch Signal-to-Noise Ratio (SNR): {snr_db:.2f} dB (Pass criterion >= 28 dB)")
    assert snr_db >= 28.0, f"SNR {snr_db:.2f} dB below 28 dB threshold!"
    print(f"-> PASS: Touch SNR ({snr_db:.2f} dB) exceeds 28 dB milestone threshold.")


def test_4_eft_impulse_immunity():
    print("\n=== Test 4: EFT / ESD +800 LSB Impulse Stress Immunity Test ===")
    np.random.seed(123)
    baseline_true = -334.0
    sigma_raw = 0.60

    kf = PythonKalmanFilter(initial_val=baseline_true, q_proc=0.35, r_base=1.0, alpha=0.75)

    deviations = []
    spike_spreads = []

    for cycle in range(200):
        burst = np.random.normal(baseline_true, sigma_raw, 9)
        # Every 10 cycles, inject a harsh +800 LSB EFT spike into one channel sample
        if cycle % 10 == 0:
            burst[np.random.randint(0, 9)] += 800.0

        z_m, spr = alpha_trimmed_mean(burst)
        x_e = kf.update(z_m, spr)

        dev = abs(x_e - baseline_true)
        deviations.append(dev)
        if cycle % 10 == 0:
            spike_spreads.append(spr)

    max_deviation = max(deviations)
    print(f"-> Maximum Deviation under +800 LSB Spikes: {max_deviation:.4f} LSB")
    print(f"-> Average Spread during Impulse Cycles:    {np.mean(spike_spreads):.2f} LSB")

    # M2 Pass Criterion: Maximum perturbation under EFT pulse < 3.0 LSB
    assert max_deviation < 3.0, f"Impulse perturbation {max_deviation:.4f} LSB exceeds 3.0 LSB limit!"
    print(f"-> PASS: +800 LSB EFT spike perturbation ({max_deviation:.4f} LSB) strictly < 3.0 LSB.")


def test_5_fast_tap_dynamic_response():
    print("\n=== Test 5: Fast Tap Dynamic Response & Step Tracking Latency ===")
    baseline = -334.0
    touch_shift = 150.0  # Typical firm tap

    kf = PythonKalmanFilter(initial_val=baseline, q_proc=0.35, r_base=1.0, alpha=0.75)

    # Simulate 20 idle cycles, then abrupt step for 5 cycles (~60 ms tap), then release
    cycles = 40
    true_signal = [baseline] * 15 + [baseline + touch_shift] * 6 + [baseline] * 19

    x_est_trace = []
    for sig in true_signal:
        burst = [sig + np.random.normal(0, 0.4) for _ in range(9)]
        z_m, spr = alpha_trimmed_mean(burst)
        x_e = kf.update(z_m, spr)
        x_est_trace.append(x_e)

    # Check latency at onset cycle 15
    tap_onset_cycle = 15
    shift_cycle_15 = x_est_trace[tap_onset_cycle] - baseline
    shift_cycle_16 = x_est_trace[tap_onset_cycle + 1] - baseline

    percent_1st_cycle = (shift_cycle_15 / touch_shift) * 100.0
    percent_2nd_cycle = (shift_cycle_16 / touch_shift) * 100.0

    print(f"-> Step Onset (+{touch_shift:.0f} LSB):")
    print(f"   Cycle 1 Response: +{shift_cycle_15:.2f} LSB ({percent_1st_cycle:.1f}% of full step)")
    print(f"   Cycle 2 Response: +{shift_cycle_16:.2f} LSB ({percent_2nd_cycle:.1f}% of full step)")

    # Detection threshold is 50 LSB; cycle 1 must immediately cross 50 LSB threshold!
    assert shift_cycle_15 >= 50.0, f"Cycle 1 shift {shift_cycle_15:.2f} did not cross 50 LSB threshold!"
    print("-> PASS: Touch threshold (50 LSB) crossed within 1 scan cycle (latency <= 1 cycle).")


def main():
    print("=" * 70)
    print("   S32K144W Touch Sensing CVD - Milestone 2 Verification Suite   ")
    print("=" * 70)

    test_1_source_code_contract()
    test_2_elf_symbol_table()
    test_3_quiet_baseline_noise_reduction()
    test_4_eft_impulse_immunity()
    test_5_fast_tap_dynamic_response()

    print("\n" + "=" * 70)
    print("   ALL TESTS PASSED: MILESTONE 2 VERIFICATION 100% SUCCESS        ")
    print("=" * 70)


if __name__ == "__main__":
    main()
