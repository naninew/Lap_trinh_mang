@echo off
setlocal
cd /d "%~dp0"

title NetChess Server
echo ============================================================
echo                  STARTING NETCHESS SERVER                  
echo ============================================================

if exist "server\server.exe" goto run_win
where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 goto run_win_compile
where wsl >nul 2>&1
if %ERRORLEVEL% EQU 0 goto run_wsl

echo [ERROR] Neither native GCC nor WSL was found.
pause
exit /b 1

:run_win_compile
call build.bat
:run_win
cd server
server.exe 8888
exit /b 0

:run_wsl
echo [*] Running server in WSL on port 8888...
wsl bash -c "cd $(wslpath '%CD%')/server && make all && ./server 8888"
exit /b 0
