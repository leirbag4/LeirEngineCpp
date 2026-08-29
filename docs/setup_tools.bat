@echo off
REM setup_tools.bat — provisiona las herramientas del sistema de docs (TODO_DOCS.md §3.1).
REM  - Doxygen 1.18.0 portátil  -> docs/tools/doxygen/
REM  - Graphviz 15.1.1 portátil -> docs/tools/graphviz/   (best-effort; opcional)
REM  - Python deps (pip install -r requirements.txt)
REM Idempotente: si el tool ya existe, no re-descarga.
REM Python: LEIR_PYTHON > path del dev > python en PATH.
setlocal
set "TOOLS=%~dp0tools"
set "DOXY_VER=1.18.0"
set "DOXY_TAG=1_18_0"
set "GV_VER=15.1.1"

if not exist "%TOOLS%" mkdir "%TOOLS%"

REM --- Python ---
if defined LEIR_PYTHON (
    set "PY=%LEIR_PYTHON%"
) else if exist "C:\programs_dev\python_3_13_7_opt_b\python.exe" (
    set "PY=C:\programs_dev\python_3_13_7_opt_b\python.exe"
) else (
    set "PY=python"
)
echo [setup] Python: %PY%
"%PY%" --version >nul 2>&1
if errorlevel 1 (
    echo [setup] ERROR: no se encontro Python. Setea LEIR_PYTHON o agrega python al PATH.
    exit /b 1
)

REM --- Doxygen (portatil) ---
if exist "%TOOLS%\doxygen\doxygen.exe" (
    echo [setup] Doxygen ya presente.
) else (
    echo [setup] Bajando Doxygen %DOXY_VER% portatil...
    curl.exe -L -sS -o "%TOOLS%\doxygen-%DOXY_VER%.zip" "https://github.com/doxygen/doxygen/releases/download/Release_%DOXY_TAG%/doxygen-%DOXY_VER%.windows.x64.bin.zip"
    if errorlevel 1 ( echo [setup] ERROR bajando Doxygen. & exit /b 1 )
    if not exist "%TOOLS%\doxygen" mkdir "%TOOLS%\doxygen"
    tar.exe -xf "%TOOLS%\doxygen-%DOXY_VER%.zip" -C "%TOOLS%\doxygen"
    if not exist "%TOOLS%\doxygen\doxygen.exe" ( echo [setup] ERROR extrayendo Doxygen. & exit /b 1 )
    echo [setup] Doxygen OK.
)

REM --- Graphviz (portatil, best-effort) ---
set "GV_DOT="
for /f "delims=" %%f in ('powershell -NoProfile -Command "(Get-ChildItem -Recurse -Filter dot.exe '%TOOLS%\graphviz' -ErrorAction SilentlyContinue | Select-Object -First 1).FullName"') do set "GV_DOT=%%f"
if defined GV_DOT (
    echo [setup] Graphviz ya presente: %GV_DOT%
) else (
    echo [setup] Bajando Graphviz %GV_VER% portatil...
    curl.exe -L -sS -o "%TOOLS%\graphviz-%GV_VER%.zip" "https://gitlab.com/api/v4/projects/4207231/packages/generic/graphviz-releases/%GV_VER%/windows_10_cmake_Release_Graphviz-%GV_VER%-win64.zip"
    if not errorlevel 1 (
        if not exist "%TOOLS%\graphviz" mkdir "%TOOLS%\graphviz"
        tar.exe -xf "%TOOLS%\graphviz-%GV_VER%.zip" -C "%TOOLS%\graphviz"
        for /f "delims=" %%f in ('powershell -NoProfile -Command "(Get-ChildItem -Recurse -Filter dot.exe '%TOOLS%\graphviz' -ErrorAction SilentlyContinue | Select-Object -First 1).FullName"') do set "GV_DOT=%%f"
        if defined GV_DOT ( echo [setup] Graphviz OK: %GV_DOT% ) else ( echo [setup] AVISO: dot.exe no encontrado tras extraer. )
    ) else (
        echo [setup] AVISO: no se pudo bajar Graphviz ^(opcional^). Diagramas sin dot.
    )
)

REM --- Python deps ---
echo [setup] Instalando dependencias de docs (pip)...
"%PY%" -m pip install --upgrade pip >nul
"%PY%" -m pip install -r "%~dp0requirements.txt"
if errorlevel 1 ( echo [setup] ERROR instalando deps. & exit /b 1 )
echo [setup] Deps OK.

echo [setup] Listo. Ahora corre generate_docs.bat.
endlocal