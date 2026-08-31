@echo off
REM generate_docs.bat — genera el sitio de documentación (TODO_DOCS.md §3.5).
REM Uso: generate_docs.bat [FILTER] [THEME] [PYGMENT]
REM   FILTER:  ALL (default) | UI | ECS | Math | RHI   — filtra Doxygen INPUT para iterar rápido
REM   THEME:   furo | book | pydata (default) | rtd  — tema Sphinx (dark)
REM   PYGMENT: default (=leir_theme) | leir_theme (default) | monokai | dracula | github-dark | native | nord | one-dark | material | solarized-dark ...
REM Ej: generate_docs.bat                 — ALL pydata leir_theme (default del proyecto)
REM     generate_docs.bat UI              — UI pydata leir_theme
REM     generate_docs.bat UI book dracula — solo UI, tema book, estilo dracula
REM     generate_docs.bat ALL furo monokai
REM
REM Nota cmd: NO usar paréntesis en textos dentro de bloques "if ( )" ni referenciar
REM     %ProgramFiles(x86)% dentro de un bloque — el parser cuenta los paréntesis.
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"

if "%~1"=="--help" goto :help
if "%~1"=="-h" goto :help
if "%~1"=="/?" goto :help

REM --- 1) tools (si faltan) ---
if not exist "docs\tools\doxygen\doxygen.exe" (
    echo [docs] Tools no presentes — corriendo setup_tools.bat...
    call "docs\setup_tools.bat"
    if errorlevel 1 ( echo [docs] ERROR en setup_tools. & popd & exit /b 1 )
)

REM --- args ---
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=ALL"
set "THEME=%~2"
if "%THEME%"=="" set "THEME=pydata"
set "PYGMENT=%~3"
if "%PYGMENT%"=="" set "PYGMENT=leir_theme"

REM --- mapear FILTER a Doxygen INPUT (incluye DocsGroups.h primero para que @ingroup exista) ---
set "DOXY_INPUT=engine/include/LeirEngine editor/src"
if /I "%FILTER%"=="UI" set "DOXY_INPUT=engine/include/LeirEngine/DocsGroups.h engine/include/LeirEngine/UI"
if /I "%FILTER%"=="ECS" set "DOXY_INPUT=engine/include/LeirEngine/DocsGroups.h engine/include/LeirEngine/ECS"
if /I "%FILTER%"=="Math" set "DOXY_INPUT=engine/include/LeirEngine/DocsGroups.h engine/include/LeirEngine/Math"
if /I "%FILTER%"=="RHI" set "DOXY_INPUT=engine/include/LeirEngine/DocsGroups.h engine/include/LeirEngine/RHI"

REM --- mapear THEME a html_theme (conf.py lo lee vía LEIR_DOCS_THEME) ---
set "LEIR_DOCS_THEME=%THEME%"
set "LEIR_DOCS_PYGMENT=%PYGMENT%"

REM --- cmake: PATH o el de VS via vswhere (fuera de bloques if) ---
set "CMAKE=cmake"
where cmake >nul 2>&1
if not errorlevel 1 goto :have_cmake
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
"%VSWHERE%" -latest -property installationPath > "%TEMP%\leir_vspath.txt" 2>nul
set /p VSPATH=<"%TEMP%\leir_vspath.txt"
if defined VSPATH set "CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
:have_cmake
set "PY=C:\programs_dev\python_3_13_7_opt_b\python.exe"
if exist "%PY%" goto :have_py
where python >nul 2>&1
if errorlevel 1 ( echo [docs] ERROR: python no encontrado. & popd & exit /b 1 )
set "PY=python"
:have_py
if defined LEIR_PYTHON set "PY=%LEIR_PYTHON%"
echo [docs] Filtro: %FILTER%  Tema: %THEME%  Pygment: %PYGMENT%
echo [docs] CMake: %CMAKE%
echo [docs] Python: %PY%

REM --- 2) target docs ---
if /I "%FILTER%"=="ALL" (
    if not exist "build\windows-debug\CMakeCache.txt" (
        echo [docs] Configurando cmake --preset windows-debug -DLEIR_BUILD_DOCS=ON...
        "%CMAKE%" --preset windows-debug -DLEIR_BUILD_DOCS=ON
        if errorlevel 1 ( echo [docs] ERROR configurando CMake. & popd & exit /b 1 )
    )
    echo [docs] Generando cmake --build build\windows-debug --target docs...
    set MSBUILDDISABLENODEREUSE=1
    "%CMAKE%" --build build\windows-debug --target docs
    if errorlevel 1 ( echo [docs] ERROR generando docs. & popd & exit /b 1 )
) else (
    echo [docs] Generando filtrado: Doxygen INPUT=%DOXY_INPUT% ...
    if not exist "docs\sphinx" mkdir "docs\sphinx" >nul 2>&1
    copy /Y "docs\Doxyfile" "docs\Doxyfile.tmp" >nul
    powershell -NoProfile -Command "$t=Get-Content 'docs\Doxyfile.tmp' -Raw; $t -replace '(?m)^INPUT\s*=.*(?:\r?\n\s*editor/src)?', ('INPUT                  = ' + $env:DOXY_INPUT) | Set-Content 'docs\Doxyfile.tmp' -NoNewline"
    rmdir /s /q "docs\sphinx\_xml" 2>nul
    if not exist "docs\sphinx\_xml" mkdir "docs\sphinx\_xml" >nul 2>&1
    rmdir /s /q "docs\sphinx\api" 2>nul
    rmdir /s /q "docs\site" 2>nul
    set "LEIR_DOCS_INPUT=%DOXY_INPUT%"
    "docs\tools\doxygen\doxygen.exe" "docs\Doxyfile.tmp"
    if errorlevel 1 ( echo [docs] ERROR Doxygen filtrado. & del "docs\Doxyfile.tmp" 2>nul & popd & exit /b 1 )
    del "docs\Doxyfile.tmp" 2>nul
    echo [docs] Sphinx filtrado: %THEME% / %PYGMENT% ...
    "%PY%" -m sphinx -E -b html "docs\sphinx" "docs\site" > "%TEMP%\sphinx.log" 2>&1
    type "%TEMP%\sphinx.log"
    findstr /C:"build succeeded" "%TEMP%\sphinx.log" >nul
    if errorlevel 1 ( echo [docs] ERROR Sphinx filtrado. & popd & exit /b 1 )
)

REM --- 3) abrir ---
if exist "docs\site\index.html" (
    start "" "docs\site\index.html"
    echo [docs] Listo: docs\site\index.html
) else (
    echo [docs] AVISO: docs\site\index.html no encontrado.
)

popd
endlocal
goto :eof

:help
echo Uso: generate_docs.bat [FILTER] [THEME] [PYGMENT]
echo   FILTER:  ALL ^(default^) ^| UI ^| ECS ^| Math ^| RHI
echo   THEME:   furo ^| book ^| pydata ^(default^) ^| rtd
echo   PYGMENT: default ^(=leir_theme^) ^| leir_theme ^(default^) ^| monokai ^| dracula ^| github-dark ^| native ^| nord ^| one-dark ^| material ^| solarized-dark
echo Ej: generate_docs.bat                 -- ALL pydata leir_theme ^(default^)
echo     generate_docs.bat UI              -- UI pydata leir_theme
echo     generate_docs.bat UI book dracula
echo     generate_docs.bat ALL furo monokai
exit /b 0
