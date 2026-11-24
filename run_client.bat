@echo off
title NetChess Client
echo ============================================================
echo                 LAUNCHING NETCHESS CLIENT                  
echo ============================================================

set UI_PATH=%CD%\client\ui\index.html

start msedge --app="file:///%UI_PATH%" 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [*] Edge app mode not available, opening in default browser...
    start "" "%UI_PATH%"
)

echo [OK] Client launched.
