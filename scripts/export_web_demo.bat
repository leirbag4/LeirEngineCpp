@echo off
rem Build + serve the WebDemo (raw-RHI M1 cube, port 8000).
setlocal
cd /d "%~dp0.."
set "PY="
if exist "C:\programs_dev\emsdk6\python" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "C:\programs_dev\emsdk6\python"') do (
        if exist "C:\programs_dev\emsdk6\python\%%D\python.exe" set "PY=C:\programs_dev\emsdk6\python\%%D\python.exe"
    )
)
if not defined PY set "PY=python"
"%PY%" "%~dp0export_web.py" --demo demo %*
if errorlevel 1 pause