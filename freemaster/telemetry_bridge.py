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
            {"name": "g_heartbeat_state", "value": heartbeat},
            {"name": "g_cycle_counter", "value": cycle},
            {"name": "g_board_online", "value": 1}
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

async def ensure_variables_defined(ws):
    """Dynamically register variables with addresses in FreeMASTER Lite server."""
    vars_to_define = [
        {"name": "g_heartbeat_state", "addr": 536839659, "type": "uint", "size": 1},
        {"name": "g_cycle_counter", "addr": 536839616, "type": "uint", "size": 4},
        {"name": "g_board_online", "addr": 536839664, "type": "uint", "size": 1}
    ]
    for v in vars_to_define:
        msg = {
            "jsonrpc": "2.0",
            "id": 50,
            "method": "DefineVariable",
            "params": [v]
        }
        await ws.send(json.dumps(msg))
        await ws.recv()

async def run_hardware_bridge(ws, port="COM5", baudrate=115200):
    print(f"[*] Starting Hardware Bridge on {port} @ {baudrate} baud...")
    loop = asyncio.get_event_loop()
    rpc_id = 200

    while True:
        ser = None
        try:
            ser = serial.Serial(port, baudrate, timeout=0.05)
            ser.reset_input_buffer()
            print(f"[+] Successfully opened {port}! Streaming hardware telemetry into FreeMASTER Lite...", flush=True)
            # Send board online notification to FreeMASTER Lite
            await ws.send(json.dumps({
                "jsonrpc": "2.0",
                "id": rpc_id,
                "method": "WriteMultipleVariables",
                "params": [[{"name": "g_board_online", "value": 1}]]
            }))
            rpc_id += 1
            await ws.recv()
        except Exception as e:
            # Report board offline to FreeMASTER Lite
            try:
                await ws.send(json.dumps({
                    "jsonrpc": "2.0",
                    "id": rpc_id,
                    "method": "WriteMultipleVariables",
                    "params": [[{"name": "g_board_online", "value": 0}, {"name": "g_heartbeat_state", "value": 0}]]
                }))
                rpc_id += 1
                await ws.recv()
            except Exception:
                pass
            print(f"[!] Waiting for S32K144W EVB on {port} ({e}). Retrying in 1s...", flush=True)
            await asyncio.sleep(1.0)
            continue

        last_print = time.time()
        last_rx_time = time.time()
        was_online = True
        packet_count = 0

        try:
            while True:
                # Read non-blocking line from serial
                try:
                    line_bytes = await loop.run_in_executor(None, ser.readline)
                except Exception as e:
                    print(f"[!] Serial read error ({e}). Board disconnected or USB unplugged.", flush=True)
                    break

                line = line_bytes.decode('ascii', errors='ignore').strip() if line_bytes else ""
                now = time.time()

                # Check for silence timeout (> 400ms: EVB powered off or paused)
                if not line or not line.startswith("D,"):
                    if was_online and (now - last_rx_time > 0.4):
                        was_online = False
                        await ws.send(json.dumps({
                            "jsonrpc": "2.0",
                            "id": rpc_id,
                            "method": "WriteMultipleVariables",
                            "params": [[{"name": "g_board_online", "value": 0}, {"name": "g_heartbeat_state", "value": 0}]]
                        }))
                        rpc_id += 1
                        await ws.recv()
                        print(f"[!] Target EVB silent (>400ms without packet). Board may be powered off or reset.", flush=True)
                    await asyncio.sleep(0.005)
                    continue

                # Valid packet arrived
                last_rx_time = now
                if not was_online:
                    was_online = True
                    print(f"[+] Target EVB packets resumed! Board is back ONLINE.", flush=True)

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
                            {"name": "g_heartbeat_state", "value": heartbeat},
                            {"name": "g_cycle_counter", "value": cycle},
                            {"name": "g_board_online", "value": 1}
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

                        if now - last_print >= 2.0:
                            status = "TOUCH (GREEN)" if touch else "IDLE (RED)"
                            hb_str = "HB:ON (BLUE)" if heartbeat else "HB:OFF"
                            print(f"[Hardware Live] Delta={dyn_delta:6.1f} LSB | Touch={touch} ({status}) | {hb_str} | Class={pred_class} | Conf={conf:.2f} | Exec={exec_us}us ({packet_count} frames)", flush=True)
                            last_print = now
                    except (ValueError, IndexError):
                        pass
        finally:
            if ser and ser.is_open:
                try:
                    ser.close()
                except Exception:
                    pass
            # Mark board offline
            try:
                await ws.send(json.dumps({
                    "jsonrpc": "2.0",
                    "id": rpc_id,
                    "method": "WriteMultipleVariables",
                    "params": [[{"name": "g_board_online", "value": 0}, {"name": "g_heartbeat_state", "value": 0}]]
                }))
                rpc_id += 1
                await ws.recv()
            except Exception:
                pass
            await asyncio.sleep(1.0)

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
                
                # Dynamically define variables with addresses
                await ensure_variables_defined(ws)
                
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
