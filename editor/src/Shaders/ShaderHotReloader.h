#pragma once

#include <LeirEngine/RHI/IShaderCompiler.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Watches the engine's .slang sources and hot-reloads changes: recompiles the
// changed shader to the active backend target (SPIR-V for Vulkan, DXIL for
// D3D12), overwrites the bytecode file the engine reads at runtime, then calls
// the reload callback so the editor can recreate the affected pipelines.
// Editor-only dev tool.
class ShaderHotReloader {
public:
    ShaderHotReloader() = default;

    void SetCompiler(Leir::RHI::IShaderCompiler* compiler) { m_Compiler = compiler; }

    // Called after at least one shader was recompiled + written to disk.
    void SetOnReload(std::function<void()> callback) { m_OnReload = std::move(callback); }

    // Poll the .slang files for changes. `target` is the active backend target
    // (derived from RenderBackend::GetShaderFileExtension()). Cheap per frame:
    // one stat per shader file.
    void Update(Leir::RHI::ShaderTarget target);

    // Record the current file states without recompiling (call once at startup
    // so the first Update() only reacts to real changes).
    void Snap();

    // Recompile every shader to `target` and trigger the reload callback.
    void ForceReload(Leir::RHI::ShaderTarget target);

private:
    struct Entry {
        std::string source; // engine/shaders/Basic.vert.slang
        std::string output; // LEIR_SHADER_DIR/Basic.vert.spv
        Leir::RHI::ShaderStage stage;
        int64_t lastMtimeSeconds = 0;
        uint64_t lastSize = 0;
    };

    void BuildEntries();

    std::vector<Entry> m_Entries;
    Leir::RHI::IShaderCompiler* m_Compiler = nullptr;
    std::function<void()> m_OnReload;
    bool m_EntriesReady = false;
};