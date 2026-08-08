@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build/windows-debug 2>&1
if %errorlevel% neq 0 (
    echo Build failed.
    pause
    exit /b %errorlevel%
)
echo Build succeeded, launching editor...
start "" "build\windows-debug\editor\Debug\LeirEngineEditor.exe"