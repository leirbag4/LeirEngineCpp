@echo off
REM generate_docs.bat — genera el sitio de documentación (TODO_DOCS.md §3.5).
REM  1) setup_tools.bat si faltan las tools (idempotente).
REM  2) cmake --build build/windows-debug --target docs  (Doxygen XML -> Sphinx site).
REM  3) abre docs\site\index.html.
REM Nota cmd: NO usar parentesis en textos dentro de bloques "if ( )" ni referenciar
REM     %ProgramFiles(x86)% dentro de un bloque — el parser cuenta los parentesis.
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"

REM --- 1) tools (si faltan) ---
if not exist "docs\tools\doxygen\doxygen.exe" (
    echo [docs] Tools no presentes — corriendo setup_tools.bat...
    call "docs\setup_tools.bat"
    if errorlevel 1 ( echo [docs] ERROR en setup_tools. & popd & exit /b 1 )
)

REM --- cmake: PATH o el de VS via vswhere (fuera de bloques if) ---
set "CMAKE=cmake"
where cmake >nul 2>&1
if not errorlevel 1 goto :have_cmake
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
"%VSWHERE%" -latest -property installationPath > "%TEMP%\leir_vspath.txt" 2>nul
set /p VSPATH=<"%TEMP%\leir_vspath.txt"
if defined VSPATH set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
:have_cmake
echo [docs] CMake: %CMAKE%

REM --- 2) target docs ---
if not exist "build\windows-debug\CMakeCache.txt" (
    echo [docs] Configurando cmake --preset windows-debug -DLEIR_BUILD_DOCS=ON...
    "%CMAKE%" --preset windows-debug -DLEIR_BUILD_DOCS=ON
    if errorlevel 1 ( echo [docs] ERROR configurando CMake. & popd & exit /b 1 )
)

echo [docs] Generando cmake --build build\windows-debug --target docs...
set MSBUILDDISABLENODEREUSE=1
"%CMAKE%" --build build\windows-debug --target docs
if errorlevel 1 ( echo [docs] ERROR generando docs. & popd & exit /b 1 )

REM --- 3) abrir ---
if exist "docs\site\index.html" (
    start "" "docs\site\index.html"
    echo [docs] Listo: docs\site\index.html
) else (
    echo [docs] AVISO: docs\site\index.html no encontrado.
)

popd
endlocal