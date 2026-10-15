#!/usr/bin/env python3
# ==================================================================================================
#   Project              : S32K144W Touch Sensing CVD Algorithm
#   Component            : FreeMASTER Hardware Telemetry Serial-to-WebSocket Bridge
#   Platform             : Python 3.12 (Windows)
#
#   Copyright (c) 2026, Max97k
#   All Rights Reserved.
# ==================================================================================================

import asyncio
import json
import math
import sys
import time
import argparse

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

import websockets

FMLITE_WS_URL = "ws://127.0.0.1:8090"

async def run_simulation(ws):
    print("[*] Running in High-Fidelity Simulation / Demo Waveform Mode...")
    cycle = 0
    baseline = 185.0
    delta = 0.0
    touch = 0
    conf = 0.0
    cls = 0
    t0 = time.time()
    last_print = time.time()
    rpc_id = 10

    while True:
        cycle += 1
        elapsed = time.time() - t0
        phase = (elapsed % 5.0)
        
        # Periodic simulated touch press every 5 seconds
        if 1.5 < phase < 3.2:
            touch = 1
            delta = 1450.0 + 20.0 * math.sin(cycle * 0.25)
            cls = 1 # Bare finger
            conf = min(0.98, conf + 0.15)
        else:
            touch = 0
            delta = 0.0 + 2.0 * math.sin(cycle * 0.1)
            cls = 0 # Idle
            conf = max(0.02, conf - 0.1)
            
        x_est = baseline + delta + (0.5 if cycle % 2 == 0 else -0.5)
        raw_diff = int(x_est)
        heartbeat = 1 if ((elapsed % 1.0) < 0.1) else 0
        
        batch = [
            {"name": "g_touch_active", "value": touch},
            {"name": "g_exec_time_us", "value": 32},
            {"name": "raw_diff", "value": raw_diff},
            {"name": "kalman_x_est", "value": round(x_est, 1)},
            {"name": "baseline_val", "value": round(baseline, 1)},
            {"name": "dynamic_delta", "value": round(delta, 1)},
            {"name": "confidence", "value": round(conf, 2)},
            {"name": "predicted_class", "value": cls},
            {"name": "dta_threshold", "value": 48.0},
            {"name": "g_heartbeat_state", "value": heartbeat}
        ]
        
        msg = json.dumps({
            "jsonrpc": "2.0",
            "id": rpc_id,
            "method": "WriteMultipleVariables",
            "params": [batch]
        })
        rpc_id += 1
        await ws.send(msg)
        await ws.recv() # Await ack
        
        if time.time() - last_print >= 2.0:
            hb_str = "HB:ON (BLUE)" if heartbeat else "HB:OFF"
            led_str = "GREEN" if touch else "RED"
            print(f"[Sim Live] Delta={delta:6.1f} LSB | Touch={touch} (LED:{led_str}) | {hb_str} | Class={cls} | Conf={conf:.2f}")
            last_print = time.time()
            
        await asyncio.sleep(0.015)

async def run_hardware_bridge(ws, port="COM5", baudrate=115200):
    print(f"[*] Connecting to S32K144W EVB on {port} @ {baudrate} baud...")
    ser = None
    try:
        ser = serial.Serial(port, baudrate, timeout=0.05)
        print(f"[+] Successfully opened {port}! Streaming hardware telemetry into FreeMASTER Lite...")
    except Exception as e:
        print(f"[!] Warning: Cannot open {port}: {e}")
        print("[*] Falling back to Simulation Mode...")
        await run_simulation(ws)
        return

    ser.reset_input_buffer()
    last_print = time.time()
    packet_count = 0
    rpc_id = 100
    loop = asyncio.get_event_loop()

    try:
        while True:
            # Read non-blocking line from serial
            line_bytes = await loop.run_in_executor(None, ser.readline)
            line = line_bytes.decode('ascii', errors='ignore').strip()
            if not line or not line.startswith("D,"):
                continue

            parts = line.split(",")
            # Format: D,<Cycle>,<A>,<B>,<DeltaD>,<Xest>,<Base>,<DynDelta>,<Class>,<Conf>,<Touch>,<ExecUs>,<Heartbeat>,<Status>
            if len(parts) >= 12:
                try:
                    cycle = int(parts[1]) if parts[1].isdigit() else 0
                    raw_diff = int(parts[4])
                    x_est = float(parts[5])
                    baseline = float(parts[6])
                    dyn_delta = float(parts[7])
                    pred_class = int(parts[8])
                    conf = float(parts[9])
                    touch = int(parts[10])
                    exec_us = int(parts[11])
                    if len(parts) >= 13 and parts[12].strip().isdigit():
                        heartbeat = int(parts[12].strip())
                    else:
                        heartbeat = 1 if ((cycle % 67) < 7) else 0

                    batch = [
                        {"name": "g_touch_active", "value": touch},
                        {"name": "g_exec_time_us", "value": exec_us},
                        {"name": "raw_diff", "value": raw_diff},
                        {"name": "kalman_x_est", "value": x_est},
                        {"name": "baseline_val", "value": baseline},
                        {"name": "dynamic_delta", "value": dyn_delta},
                        {"name": "confidence", "value": conf},
                        {"name": "predicted_class", "value": pred_class},
                        {"name": "dta_threshold", "value": 48.0},
                        {"name": "g_heartbeat_state", "value": heartbeat}
                    ]

                    msg = json.dumps({
                        "jsonrpc": "2.0",
                        "id": rpc_id,
                        "method": "WriteMultipleVariables",
                        "params": [batch]
                    })
                    rpc_id += 1
                    await ws.send(msg)
                    await ws.recv() # Await ack
                    packet_count += 1

                    if time.time() - last_print >= 2.0:
                        status = "TOUCH (GREEN)" if touch else "IDLE (RED)"
                        hb_str = "HB:ON (BLUE)" if heartbeat else "HB:OFF"
                        print(f"[Hardware Live] Delta={dyn_delta:6.1f} LSB | Touch={touch} ({status}) | {hb_str} | Class={pred_class} | Conf={conf:.2f} | Exec={exec_us}us ({packet_count} frames)", flush=True)
                        last_print = time.time()
                except (ValueError, IndexError):
                    pass
    except asyncio.CancelledError:
        pass
    finally:
        if ser and ser.is_open:
            ser.close()

async def main():
    parser = argparse.ArgumentParser(description="FreeMASTER WebSocket Telemetry Bridge")
    parser.add_argument("--port", default="COM5", help="Target serial port (default: COM5)")
    parser.add_argument("--baud", default=115200, type=int, help="Baud rate (default: 115200)")
    parser.add_argument("--sim", action="store_true", help="Force simulation mode")
    args = parser.parse_args()

    print(f"[*] Connecting to FreeMASTER Lite WebSocket at {FMLITE_WS_URL}...")
    while True:
        try:
            async with websockets.connect(FMLITE_WS_URL) as ws:
                print("[+] Connected to FreeMASTER Lite!")
                
                # Activate named connection
                await ws.send(json.dumps({
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "StartComm",
                    "params": ["S32K144W_OpenSDA"]
                }))
                await ws.recv()
                
                if args.sim or not HAS_SERIAL:
                    await run_simulation(ws)
                else:
                    await run_hardware_bridge(ws, port=args.port, baudrate=args.baud)
        except (websockets.exceptions.ConnectionClosed, ConnectionRefusedError, OSError) as e:
            print(f"[!] FreeMASTER Lite connection failed ({e}). Retrying in 2 seconds...")
            await asyncio.sleep(2)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[*] Bridge stopped by user.")
