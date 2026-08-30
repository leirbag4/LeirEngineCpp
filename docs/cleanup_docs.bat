@echo off
REM cleanup_docs.bat — borra todo lo regenerable del sistema de docs (TODO_DOCS.md).
REM Se puede regenerar por completo con docs\generate_docs.bat (o cmake --build --target docs).
REM No toca tools/ (portátiles) ni las guías .md escritas a mano.
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"

echo [cleanup] Borrando artefactos regenerables...

if exist "docs\site" (
    rmdir /s /q "docs\site"
    echo [cleanup] - docs\site
)
if exist "docs\sphinx\api" (
    rmdir /s /q "docs\sphinx\api"
    echo [cleanup] - docs\sphinx\api (Exhale)
)
if exist "docs\sphinx\_xml" (
    rmdir /s /q "docs\sphinx\_xml"
    echo [cleanup] - docs\sphinx\_xml (Doxygen XML)
)
:: Por si quedó cache fuera de site (Sphinx -E lo ignora, pero lo limpiamos)
if exist "docs\sphinx\.doctrees" rmdir /s /q "docs\sphinx\.doctrees" 2>nul

echo [cleanup] Listo. Corré docs\generate_docs.bat para regenerar.
popd
endlocal
