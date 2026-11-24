@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo           NETCHESS - COMPILING C SERVER AND CLI            
echo ============================================================

where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 goto compile_win

where wsl >nul 2>&1
if %ERRORLEVEL% EQU 0 goto compile_wsl

echo [ERROR] Neither GCC nor WSL found in system PATH.
goto end_err

:compile_win
echo [*] Compiling C Server with Windows GCC...
gcc -O2 -Wall -Wextra -Iserver/include server/src/*.c -o server/server.exe -lws2_32
if %ERRORLEVEL% NEQ 0 goto end_err

echo [*] Compiling C CLI Client with Windows GCC...
gcc -O2 -Wall -Wextra client/client_cli/client_cli.c -o client/client_cli/client_cli.exe -lws2_32
if %ERRORLEVEL% NEQ 0 goto end_err

echo.
echo [SUCCESS] Windows native compilation completed successfully!
goto end_ok

:compile_wsl
echo [*] Native Windows GCC not found. Compiling via WSL...
wsl bash -c "cd $(wslpath '%CD%') && make clean && make all"
if %ERRORLEVEL% NEQ 0 goto end_err

echo.
echo [SUCCESS] WSL compilation completed successfully!
goto end_ok

:end_ok
echo ============================================================
pause
exit /b 0

:end_err
echo.
echo [ERROR] Compilation failed.
echo ============================================================
pause
exit /b 1
