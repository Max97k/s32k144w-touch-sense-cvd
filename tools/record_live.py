#!/usr/bin/env python3
# Copyright (c) 2026, Max97k
# All Rights Reserved.
# SPDX-License-Identifier: BSD-3-Clause
"""Record live touch telemetry (D,... CSV lines) from COM5 to a CSV file.

Usage: python tools/record_live.py [seconds=30] [out.csv]

Copyright (c) 2026, Max97k. All rights reserved.
"""
import sys
import time
import serial

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
out = sys.argv[2] if len(sys.argv) > 2 else "data/hardware/m_live_recording.csv"
HDR = "cycle,A,B,deltaD,xest,base,dyn,cls,conf,touch,exec_us,hb,status"

ser = serial.Serial("COM5", 115200, timeout=0.2)
ser.reset_input_buffer()
t0 = time.time()
n = 0
with open(out, "w") as f:
    f.write("t_s," + HDR + "\n")
    while time.time() - t0 < secs:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("D,") and line.count(",") in (12, 13):
            f.write("%.3f,%s\n" % (time.time() - t0, line[2:]))
            n += 1
ser.close()
print("recorded %d frames -> %s" % (n, out))
