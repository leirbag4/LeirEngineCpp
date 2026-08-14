#include "ShaderHotReloader.h"
#include "ShaderExporter.h"

#include <LeirEngine/Core/Log.h>

#include <chrono>
#include <cstdio>
#include <filesystem>

namespace {

const char* kShaderSourceDir = LEIR_SHADER_SOURCE_DIR;
const char* kShaderOutputDir = LEIR_SHADER_DIR;

const char* SourceExtension(Leir::RHI::ShaderTarget target)
{
    return target == Leir::RHI::ShaderTarget::DXIL ? ".dxil" : ".spv";
}

} // namespace

void ShaderHotReloader::BuildEntries()
{
    m_Entries.clear();
    struct FileDef { const char* name; Leir::RHI::ShaderStage stage; };
    static const FileDef kFiles[] = {
        { "Basic.vert", Leir::RHI::ShaderStage::Vertex },
        { "Basic.frag", Leir::RHI::ShaderStage::Fragment },
        { "Sprite.vert", Leir::RHI::ShaderStage::Vertex },
        { "Sprite.frag", Leir::RHI::ShaderStage::Fragment },
        { "UI.vert", Leir::RHI::ShaderStage::Vertex },
        { "UI.frag", Leir::RHI::ShaderStage::Fragment },
    };
    for (const auto& def : kFiles) {
        Entry entry;
        entry.source = std::string(kShaderSourceDir) + "/" + def.name + ".slang";
        entry.stage = def.stage;
        m_Entries.push_back(std::move(entry));
    }
    m_EntriesReady = true;
}

void ShaderHotReloader::Snap()
{
    if (!m_EntriesReady)
        BuildEntries();
    for (auto& entry : m_Entries) {
        std::error_code ec;
        const auto fileTime = std::filesystem::last_write_time(entry.source, ec);
        entry.lastMtimeSeconds = ec ? 0 : std::chrono::duration_cast<std::chrono::seconds>(
            fileTime.time_since_epoch()).count();
        entry.lastSize = std::filesystem::file_size(entry.source, ec);
    }
}

void ShaderHotReloader::ForceReload(Leir::RHI::ShaderTarget target)
{
    if (!m_EntriesReady)
        BuildEntries();
    for (auto& entry : m_Entries) {
        entry.lastMtimeSeconds = 0;
        entry.lastSize = 0;
    }
    Update(target);
}

void ShaderHotReloader::Update(Leir::RHI::ShaderTarget target)
{
    if (!m_Compiler || !m_Compiler->IsAvailable())
        return;

    if (!m_EntriesReady)
        BuildEntries();

    bool anyChanged = false;
    for (auto& entry : m_Entries) {
        std::error_code ec;
        const auto fileTime = std::filesystem::last_write_time(entry.source, ec);
        const uintmax_t size = std::filesystem::file_size(entry.source, ec);
        if (ec)
            continue;

        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            fileTime.time_since_epoch()).count();
        if (seconds == entry.lastMtimeSeconds && size == entry.lastSize)
            continue;

        entry.lastMtimeSeconds = seconds;
        entry.lastSize = size;

        auto result = m_Compiler->Compile(entry.source, target, entry.stage, /*reflect=*/true);
        if (!result.ok) {
            Leir::XConsole::PrintError("[HotReload] {} FAILED: {}", entry.source, result.error);
            continue;
        }

        entry.output = std::string(kShaderOutputDir) + "/" +
            entry.source.substr(entry.source.find_last_of("/\\") + 1);
        if (entry.output.size() > 6 &&
            entry.output.compare(entry.output.size() - 6, 6, ".slang") == 0)
            entry.output.resize(entry.output.size() - 6);
        entry.output += SourceExtension(target);

        std::FILE* fp = std::fopen(entry.output.c_str(), "wb");
        if (!fp) {
            Leir::XConsole::PrintError("[HotReload] cannot write {}", entry.output);
            continue;
        }
        std::fwrite(result.bytecode.data(), 1, result.bytecode.size(), fp);
        std::fclose(fp);

        // Regenerate the reflection sidecar alongside the bytecode so pipeline
        // layouts keep matching the shader signature after a reload.
        const std::string base = entry.source.substr(entry.source.find_last_of("/\\") + 1);
        const std::string name = base.size() > 6 ? base.substr(0, base.size() - 6) : base;
        ShaderExporter::WriteReflectionSidecar(name, result.reflection, entry.stage,
            kShaderOutputDir);

        Leir::XConsole::Println("[HotReload] {} -> {} ({} bytes)",
            entry.source, entry.output, result.bytecode.size());
        anyChanged = true;
    }

    if (anyChanged && m_OnReload)
        m_OnReload();
}
