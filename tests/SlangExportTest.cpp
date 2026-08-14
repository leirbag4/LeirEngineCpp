#include <LeirEngine/Core/Log.h>
#include <LeirEngine/RHI/IShaderCompiler.h>

#include "ShaderExporter.h"
#include "SlangShaderCompiler.h"

#include <cstdio>
#include <filesystem>
#include <string>

// CI smoke test: run the vendored Slang compiler through the editor's
// ShaderExporter on all 5 targets. Fails (non-zero exit) unless every target
// produced all 6 shaders. Exercises link + runtime loading + codegen of the
// vendored libslang on every platform (Windows DLL, Linux .so, macOS .dylib).
int main()
{
    Leir::RHI::SlangShaderCompiler compiler;
    if (!compiler.IsAvailable()) {
        std::fprintf(stderr, "SlangExportTest: shader compiler unavailable\n");
        return 1;
    }
    std::printf("SlangExportTest: slang %s\n", compiler.GetVersion().c_str());

    const auto lines = ShaderExporter::ExportAll(&compiler);
    int fullTargets = 0;
    bool anyFailure = false;
    for (const auto& line : lines) {
        std::printf("%s\n", line.c_str());
        if (line.find("/6 shaders") != std::string::npos)
            ++fullTargets;
        if (line.find("FAILED") != std::string::npos ||
            line.find("failed") != std::string::npos)
            anyFailure = true;
    }

    // Expect one "N/6 shaders" line per target: SPIR-V, Metal, WGSL, GLSL 450
    // everywhere; DXIL additionally on Windows (it needs the external dxc,
    // which only the Windows Vulkan SDK provides).
#ifdef _WIN32
    const int kExpectedTargets = 5;
#else
    const int kExpectedTargets = 4;
#endif
    if (fullTargets != kExpectedTargets || anyFailure) {
        std::fprintf(stderr, "SlangExportTest: FAILED (full targets=%d/%d, failures=%s)\n",
            fullTargets, kExpectedTargets, anyFailure ? "yes" : "no");
        return 1;
    }

    // ExportAll must also emit one reflection sidecar per shader (the SpirV
    // pass writes <name>.reflect.json next to the bytecode) — the engine
    // derives its pipeline layouts from these at runtime (Plan B, Fase 2).
    int sidecars = 0;
    std::error_code ec;
    const std::string sidecarDir = std::string(LEIR_SHADER_EXPORT_DIR) + "/spirv";
    for (const auto& entry : std::filesystem::directory_iterator(sidecarDir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() > 13 && name.compare(name.size() - 13, 13, ".reflect.json") == 0)
            ++sidecars;
    }
    if (ec || sidecars != 6) {
        std::fprintf(stderr, "SlangExportTest: FAILED (reflection sidecars=%d/6)\n", sidecars);
        return 1;
    }

    std::printf("SlangExportTest: OK (%d reflection sidecars)\n", sidecars);
    return 0;
}