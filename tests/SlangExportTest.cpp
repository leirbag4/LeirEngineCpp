#include <LeirEngine/Core/Log.h>
#include <LeirEngine/RHI/IShaderCompiler.h>

#include "ShaderExporter.h"
#include "SlangShaderCompiler.h"

#include <cstdio>
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

    // Expect one "N/6 shaders" line per target (SPIR-V, DXIL, Metal, WGSL,
    // GLSL 450) and zero failures.
    if (fullTargets != 5 || anyFailure) {
        std::fprintf(stderr, "SlangExportTest: FAILED (full targets=%d, failures=%s)\n",
            fullTargets, anyFailure ? "yes" : "no");
        return 1;
    }
    std::printf("SlangExportTest: OK\n");
    return 0;
}