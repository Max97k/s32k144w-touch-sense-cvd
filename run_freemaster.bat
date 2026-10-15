@echo off
title S32K144W CVD Touch Sensing Engine - FreeMASTER Dashboard Launcher
echo ===============================================================================
echo     S32K144W Automotive Capacitive Touch Sensing CVD Engine
echo     FreeMASTER Lite 1.5.1 Live Oscilloscope and Diagnostics Launcher
echo ===============================================================================
echo.

cd /d "%~dp0freemaster"

:: 1. Terminate any previous fmlite instance to release port 8090
taskkill /F /IM fmlite.exe >nul 2>&1
timeout /t 1 /nobreak >nul

:: 2. Launch FreeMASTER Lite server in background
echo [*] Starting FreeMASTER Lite Server (Port 8090)...
start "FreeMASTER Lite Server" /B "C:\NXP\FreeMASTER Lite 1.5.1\FreeMASTER Lite\fmlite.exe" touch_freemaster.fmcfg --no-node_red --no-open_path

:: Wait for FreeMASTER Lite HTTP/WebSocket server to initialize
timeout /t 2 /nobreak >nul

:: 3. Launch Python Telemetry Serial Bridge (COM5 @ 115200 baud -> UDP 9876)
echo [*] Starting Hardware Telemetry Serial Bridge (COM5 @ 115200 baud)...
start "S32K144W Telemetry Bridge" python telemetry_bridge.py --port COM5 --baud 115200

:: 4. Open Browser Dashboard
echo [*] Launching Real-Time Touch Scope Dashboard in Browser...
start "" "http://localhost:8090/touch_scope.html"

echo.
echo ===============================================================================
echo [+] FreeMASTER Lite and Telemetry Bridge are RUNNING!
echo     - Dashboard:   http://localhost:8090/touch_scope.html
echo     - JSON-RPC:    http://localhost:8090/jsonrpc
echo     - AI MCP:      Active on ws://localhost:8090
echo.
echo Press any key to stop all FreeMASTER services...
echo ===============================================================================
pause >nul

echo [*] Shutting down FreeMASTER Lite and Bridge...
taskkill /F /IM fmlite.exe >nul 2>&1
taskkill /F /FI "WINDOWTITLE eq S32K144W Telemetry Bridge*" >nul 2>&1
echo [✓] Done.
