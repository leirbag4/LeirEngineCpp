cmake_minimum_required(VERSION 3.20)
project(LeirEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(LEIR_BUILD_EDITOR "Build the LeirEngineEditor" ON)
option(LEIR_BUILD_TESTS "Build unit tests" OFF)
option(LEIR_BUILD_EXAMPLES "Build example projects" OFF)
option(LEIR_BUILD_DOCS "Build Doxygen documentation" OFF)

# Dependencies
add_subdirectory(dependencies)

# Engine library
add_subdirectory(engine)

# Editor
if(LEIR_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

# Tests
if(LEIR_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Examples
if(LEIR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
