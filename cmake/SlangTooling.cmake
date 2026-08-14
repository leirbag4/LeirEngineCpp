# Slang tooling wiring shared by the editor and the SlangExportTest smoke test.
#
# Links the vendored Slang compiler (editor/vendor/slang/, v2026.14.1 — see
# its README for provenance and update steps) to a target and deploys the
# runtime shared libraries next to its exe at build time:
#   - Windows: link slang-compiler.lib, copy the 4 runtime DLLs.
#   - Linux:   link libslang-compiler.so.0.2026.14.1, rpath $ORIGIN, copy the
#              4 .so files. The compiler's own RUNPATH ($ORIGIN/../lib:$ORIGIN)
#              finds its dlopen'd modules (glslang / glsl-module) next to the exe.
#   - macOS:   link libslang-compiler.0.2026.14.1.dylib, rpath @loader_path,
#              copy the 4 .dylib files (install_name is @rpath/...).
#
# No deprecated proxies: the legacy slang.dll / libslang.so proxy names are NOT
# vendored or linked (removed in upstream). Link slang-compiler / slang-rt.
function(leir_setup_slang_target TARGET_NAME)
    set(LEIR_SLANG_ROOT "${CMAKE_SOURCE_DIR}/editor/vendor/slang")

    target_include_directories(${TARGET_NAME} PRIVATE "${LEIR_SLANG_ROOT}/include")

    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "${LEIR_SLANG_ROOT}/windows/slang-compiler.lib")
        foreach(SLANG_DLL IN ITEMS
                slang-compiler.dll slang-rt.dll slang-glslang.dll slang-glsl-module.dll)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LEIR_SLANG_ROOT}/windows/${SLANG_DLL}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SLANG_DLL}"
            )
        endforeach()
    elseif(APPLE)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "${LEIR_SLANG_ROOT}/macos/libslang-compiler.0.2026.14.1.dylib")
        # @loader_path resolves against the binary that loaded the dylib (the
        # exe dir), so the Slang dylibs next to the exe are found. The engine
        # dir is included so the target also finds LeirEngine.dylib.
        set_target_properties(${TARGET_NAME} PROPERTIES
            BUILD_RPATH "@loader_path;${CMAKE_BINARY_DIR}/engine")
        foreach(SLANG_DYLIB IN ITEMS
                libslang-compiler.0.2026.14.1.dylib libslang-rt.0.2026.14.1.dylib
                libslang-glslang-2026.14.1.dylib libslang-glsl-module-2026.14.1.dylib)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LEIR_SLANG_ROOT}/macos/${SLANG_DYLIB}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SLANG_DYLIB}"
            )
        endforeach()
    elseif(UNIX)
        target_link_libraries(${TARGET_NAME} PRIVATE
            "${LEIR_SLANG_ROOT}/linux/libslang-compiler.so.0.2026.14.1")
        # $ORIGIN resolves to the exe dir: the .so files copied next to the exe
        # are found (DT_NEEDED = the SONAME libslang-compiler.so.0.2026.14.1).
        # The engine dir is included so the target also finds LeirEngine.so.
        set_target_properties(${TARGET_NAME} PROPERTIES
            BUILD_RPATH "$ORIGIN;${CMAKE_BINARY_DIR}/engine")
        foreach(SLANG_SO IN ITEMS
                libslang-compiler.so.0.2026.14.1 libslang-rt.so.0.2026.14.1
                libslang-glslang-2026.14.1.so libslang-glsl-module-2026.14.1.so)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LEIR_SLANG_ROOT}/linux/${SLANG_SO}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/${SLANG_SO}"
            )
        endforeach()
    endif()
endfunction()
