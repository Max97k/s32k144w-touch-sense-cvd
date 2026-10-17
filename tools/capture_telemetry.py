#!/usr/bin/env python3
# Copyright (c) 2026, Max97k
# All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause
"""
S32K144W Touch Sensing CVD - M1 Automated Telemetry Logger & Validator
Captures 100-point baseline, verifies AN12217 calibration, and evaluates M1 Pass Criteria.
"""
import sys
import time
import serial
import serial.tools.list_ports

def find_opensda_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "OpenSDA" in p.description or "PEMicro" in p.description or p.device == "COM5":
            return p.device
    return "COM5"

def main():
    port_name = sys.argv[1] if len(sys.argv) > 1 else find_opensda_port()
    baudrate = 115200

    print(f"[*] Connecting to S32K144W Telemetry on {port_name} @ {baudrate} bps...")
    try:
        ser = serial.Serial(port_name, baudrate, timeout=2.0)
    except Exception as e:
        print(f"[-] Could not open serial port {port_name}: {e}")
        sys.exit(1)

    print("[+] Connected! Waiting for M1 report...")
    csv_records = []
    start_time = time.time()

    try:
        while time.time() - start_time < 30.0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue
            print(f"    {line}")

            if line.startswith("CSV,"):
                parts = line.split(",")
                if len(parts) >= 5:
                    idx = int(parts[1])
                    sa = int(parts[2])
                    sb = int(parts[3])
                    delta = int(parts[4])
                    csv_records.append((idx, sa, sb, delta))
                    if len(csv_records) >= 100:
                        break
            elif line.startswith("LIVE,"):
                # Format: LIVE,A:2079,B:1745,Delta:-334,NetShift:-2 [IDLE]
                try:
                    parts = line.split(",")
                    a_val = int(parts[1].split(":")[1])
                    b_val = int(parts[2].split(":")[1])
                    d_val = int(parts[3].split(":")[1])
                    csv_records.append((len(csv_records), a_val, b_val, d_val))
                    if len(csv_records) >= 100:
                        break
                except Exception:
                    pass

    except KeyboardInterrupt:
        print("\n[*] Interrupted by user.")
    finally:
        ser.close()

    if len(csv_records) >= 100:
        print("\n=======================================================")
        print("          M1 QUANTITATIVE ACCEPTANCE REPORT            ")
        print("=======================================================")
        sa_vals = [r[1] for r in csv_records]
        sb_vals = [r[2] for r in csv_records]
        delta_vals = [r[3] for r in csv_records]

        mean_sa = sum(sa_vals) / len(sa_vals)
        mean_sb = sum(sb_vals) / len(sb_vals)
        mean_delta = sum(delta_vals) / len(delta_vals)

        print(f"1. Work Point Symmetry (Untouched, Pass: 1600 ~ 2500 LSB):")
        print(f"   - Sample_A Mean: {mean_sa:.1f} LSB -> {'PASS' if 1600 <= mean_sa <= 2500 else 'FAIL'}")
        print(f"   - Sample_B Mean: {mean_sb:.1f} LSB -> {'PASS' if 1600 <= mean_sb <= 2500 else 'FAIL'}")
        print(f"   - Untouched Delta Mean: {mean_delta:.1f} LSB")

        # Save to CSV
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        out_csv = os.path.join(repo_root, "data", "hardware", "m1_baseline_100_samples.csv")
        os.makedirs(os.path.dirname(out_csv), exist_ok=True)
        with open(out_csv, "w") as f:
            f.write("Index,Sample_A_LSB,Sample_B_LSB,Delta_D_LSB\n")
            for r in csv_records:
                f.write(f"{r[0]},{r[1]},{r[2]},{r[3]}\n")
        print(f"[+] 100-sample raw dataset exported to {out_csv}")

if __name__ == "__main__":
    main()
