# DocsTooling.cmake — target `docs` del sistema de documentación (TODO_DOCS.md §3.4).
# Pipeline: Doxygen (XML) -> Sphinx (Breathe/Exhale/MyST/Furo) -> docs/site.
#   cmake --build build/windows-debug --target docs
# EXCLUDE_FROM_ALL: el build normal NO genera docs (solo --target docs).
# Herramientas: portátiles en docs/tools (doxygen), o en PATH; Python vía
# LEIR_PYTHON / path del dev (AGENTS.md) / python en PATH.
option(LEIR_BUILD_DOCS "Build the docs target (Doxygen -> XML -> Sphinx site)" ON)

if(NOT LEIR_BUILD_DOCS)
    return()
endif()

# ---- Doxygen: preferir el portátil de docs/tools; si no, buscar en el sistema ----
if(NOT DOXYGEN_EXECUTABLE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/docs/tools/doxygen/doxygen.exe")
        set(DOXYGEN_EXECUTABLE "${CMAKE_SOURCE_DIR}/docs/tools/doxygen/doxygen.exe"
            CACHE FILEPATH "Portable Doxygen (docs/tools)")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/docs/tools/doxygen/bin/doxygen.exe")
        set(DOXYGEN_EXECUTABLE "${CMAKE_SOURCE_DIR}/docs/tools/doxygen/bin/doxygen.exe"
            CACHE FILEPATH "Portable Doxygen (docs/tools)")
    endif()
endif()
find_package(Doxygen QUIET)

# ---- Python: LEIR_PYTHON > path del dev > python en PATH ----
if(NOT Python3_EXECUTABLE)
    if(DEFINED ENV{LEIR_PYTHON} AND EXISTS "$ENV{LEIR_PYTHON}")
        set(Python3_EXECUTABLE "$ENV{LEIR_PYTHON}" CACHE FILEPATH "Python (docs)")
    elseif(EXISTS "C:/programs_dev/python_3_13_7_opt_b/python.exe")
        set(Python3_EXECUTABLE "C:/programs_dev/python_3_13_7_opt_b/python.exe"
            CACHE FILEPATH "Python (docs)")
    endif()
endif()
find_package(Python3 QUIET COMPONENTS Interpreter)

if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs_api
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/docs/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "docs: Doxygen XML (engine + editor headers)"
        EXCLUDE_FROM_ALL
    )
else()
    message(WARNING "Doxygen no encontrado — target docs_api deshabilitado (corré docs/setup_tools.bat)")
endif()

if(Python3_EXECUTABLE)
    add_custom_target(docs_sphinx
        COMMAND "${Python3_EXECUTABLE}" -m sphinx -b html
                "${CMAKE_SOURCE_DIR}/docs/sphinx"
                "${CMAKE_SOURCE_DIR}/docs/site"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "docs: Sphinx build -> docs/site"
        EXCLUDE_FROM_ALL
    )
else()
    message(WARNING "Python no encontrado — target docs_sphinx deshabilitado (corré docs/setup_tools.bat)")
endif()

# docs = docs_api (doxygen) + docs_sphinx (sphinx). No se construye por defecto.
add_custom_target(docs
    DEPENDS docs_api docs_sphinx
    COMMENT "docs: Doxygen -> Sphinx (abrir docs/site/index.html)"
    EXCLUDE_FROM_ALL
)
if(TARGET docs_sphinx AND TARGET docs_api)
    add_dependencies(docs_sphinx docs_api)
endif()