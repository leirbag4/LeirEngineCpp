#include "LeirEngine/Rendering/ShaderLayout.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>

// Canonical reflection sidecar format (written by the editor shader tooling
// and parsed here at runtime):
//
//   {
//     "stage": "Vertex",
//     "bindings": [
//       { "name": "ubo", "set": 0, "binding": 0,
//         "type": "UniformBuffer", "count": 1, "stage": "Vertex" }
//     ],
//     "pushConstants": [
//       { "stage": "Vertex", "offset": 0, "size": 128 }
//     ]
//   }
//
// One sidecar per compiled stage; the engine merges them across stages. This
// is the source of truth for pipeline layouts: layouts are derived from it,
// so a "root signature doesn't match the shader" class of errors is removed
// by construction.

namespace Leir {
namespace {

const char* TypeToString(RHI::DescriptorType t)
{
    switch (t) {
        case RHI::DescriptorType::CombinedImageSampler: return "CombinedImageSampler";
        case RHI::DescriptorType::UniformBuffer: return "UniformBuffer";
    }
    return "CombinedImageSampler";
}

RHI::DescriptorType TypeFromString(const std::string& s)
{
    return s == "UniformBuffer" ? RHI::DescriptorType::UniformBuffer
                                : RHI::DescriptorType::CombinedImageSampler;
}

const char* StageToString(RHI::ShaderStage s)
{
    switch (s) {
        case RHI::ShaderStage::Vertex: return "Vertex";
        case RHI::ShaderStage::Fragment: return "Fragment";
    }
    return "Vertex";
}

RHI::ShaderStage StageFromString(const std::string& s)
{
    return s == "Fragment" ? RHI::ShaderStage::Fragment : RHI::ShaderStage::Vertex;
}

const char* StageMaskToString(RHI::ShaderStageMask m)
{
    const uint8_t v = static_cast<uint8_t>(m);
    if (v == (uint8_t)RHI::ShaderStageMask::VertexFragment)
        return "VertexFragment";
    if (v == (uint8_t)RHI::ShaderStageMask::Vertex)
        return "Vertex";
    if (v == (uint8_t)RHI::ShaderStageMask::Fragment)
        return "Fragment";
    return "VertexFragment";
}

RHI::ShaderStageMask StageMaskFromString(const std::string& s)
{
    if (s == "Vertex")
        return RHI::ShaderStageMask::Vertex;
    if (s == "Fragment")
        return RHI::ShaderStageMask::Fragment;
    return RHI::ShaderStageMask::VertexFragment;
}

// ".../Basic.vert.spv" / ".dxil" / ".web.wgsl" -> ".../Basic.vert.reflect.json"
std::string SidecarPathFor(const std::string& bytecodePath)
{
    std::string base = bytecodePath;
    static const char* const kExtensions[] = { ".web.wgsl", ".dxil", ".spv" };
    for (const char* ext : kExtensions) {
        const size_t len = std::strlen(ext);
        if (base.size() > len && base.compare(base.size() - len, len, ext) == 0) {
            base.resize(base.size() - len);
            break;
        }
    }
    return base + ".reflect.json";
}

} // namespace

RHI::ShaderReflection LoadShaderReflectionFromSidecars(
    const std::vector<std::string>& bytecodePaths)
{
    RHI::ShaderReflection merged;
    for (const auto& path : bytecodePaths) {
        const std::string sidecar = SidecarPathFor(path);
        std::ifstream in(sidecar);
        if (!in.is_open())
            continue; // no sidecar -> reflection for this stage stays empty

        nlohmann::json j;
        try {
            in >> j;
        } catch (...) {
            XConsole::PrintWarning("[Reflection] invalid sidecar {}", sidecar);
            continue;
        }

        for (const auto& b : j.value("bindings", nlohmann::json::array())) {
            RHI::ShaderBinding sb;
            sb.name = b.value("name", std::string());
            sb.set = b.value("set", 0u);
            sb.binding = b.value("binding", 0u);
            sb.type = TypeFromString(b.value("type", std::string("CombinedImageSampler")));
            sb.count = b.value("count", 1u);
            sb.stage = StageFromString(b.value("stage", std::string("Vertex")));
            merged.bindings.push_back(std::move(sb));
        }
        for (const auto& pc : j.value("pushConstants", nlohmann::json::array())) {
            RHI::ShaderPushConstantRange range;
            range.stage = StageMaskFromString(pc.value("stage", std::string("VertexFragment")));
            range.offset = pc.value("offset", 0u);
            range.size = pc.value("size", 0u);
            merged.pushConstants.push_back(range);
        }
    }
    return merged;
}

std::vector<RHISetLayoutEntry> CreateSetLayoutsFromReflection(
    RHI::RenderBackend* device, const RHI::ShaderReflection& reflection)
{
    std::vector<RHISetLayoutEntry> out;
    if (reflection.bindings.empty())
        return out;

    // Shader signature -> set layout binding. An unbounded runtime array
    // (count == UINT32_MAX) becomes a bindless binding bounded by the
    // backend's bindless table size.
    auto toBinding = [device](const RHI::ShaderBinding& b) {
        const bool bindless = b.count == UINT32_MAX;
        RHI::RHIDescriptorBinding out;
        out.binding = b.binding;
        out.type = b.type;
        out.count = bindless ? device->GetBindlessMaxTextures() : b.count;
        out.stage = b.stage;
        out.bindless = bindless;
        return out;
    };

    std::map<uint32_t, std::vector<RHI::RHIDescriptorBinding>> bySet;
    for (const auto& b : reflection.bindings)
        bySet[b.set].push_back(toBinding(b));

    // std::map iterates sets ascending -> positional set order for the backends.
    for (auto& [set, bindings] : bySet) {
        std::sort(bindings.begin(), bindings.end(),
            [](const RHI::RHIDescriptorBinding& a, const RHI::RHIDescriptorBinding& b) {
                return a.binding < b.binding;
            });
        RHISetLayoutEntry entry;
        entry.set = set;
        entry.layout = device->CreateDescriptorSetLayout(bindings);
        out.push_back(std::move(entry));
    }
    return out;
}

RHI::RHIPipelineLayout CreatePipelineLayoutFromReflection(
    RHI::RenderBackend* device, const RHI::ShaderReflection& reflection,
    const std::vector<RHISetLayoutEntry>& setLayouts)
{
    if (reflection.bindings.empty())
        return RHI::RHIPipelineLayout{};

    // Positional set order (pipeline-layout index == set number).
    std::map<uint32_t, RHI::RHIDescriptorSetLayout> bySet;
    for (const auto& e : setLayouts)
        bySet[e.set] = e.layout;

    auto toBinding = [device](const RHI::ShaderBinding& b) {
        const bool bindless = b.count == UINT32_MAX;
        RHI::RHIDescriptorBinding out;
        out.binding = b.binding;
        out.type = b.type;
        out.count = bindless ? device->GetBindlessMaxTextures() : b.count;
        out.stage = b.stage;
        out.bindless = bindless;
        return out;
    };

    std::map<uint32_t, std::vector<RHI::RHIDescriptorBinding>> sigBindings;
    for (const auto& b : reflection.bindings)
        sigBindings[b.set].push_back(toBinding(b));

    std::vector<RHI::RHIDescriptorSetLayout> layouts;
    for (const auto& [set, layout] : bySet) {
        ValidateSetLayoutAgainstReflection(reflection, set, sigBindings[set],
            device->GetBindlessMaxTextures());
        layouts.push_back(layout);
    }

    // Merge push-constant ranges with identical offset+size into one range with
    // combined stage flags (required by Vulkan for overlapping ranges).
    std::vector<RHI::RHIPushConstantRange> ranges;
    for (const auto& pc : reflection.pushConstants) {
        auto it = std::find_if(ranges.begin(), ranges.end(),
            [&](const RHI::RHIPushConstantRange& r) {
                return r.offset == pc.offset && r.size == pc.size;
            });
        if (it != ranges.end())
            it->stage = static_cast<RHI::ShaderStageMask>(
                (uint8_t)it->stage | (uint8_t)pc.stage);
        else
            ranges.push_back({ pc.stage, pc.offset, pc.size });
    }

    return device->CreatePipelineLayout(layouts, ranges);
}

bool ValidateSetLayoutAgainstReflection(
    const RHI::ShaderReflection& reflection, uint32_t set,
    const std::vector<RHI::RHIDescriptorBinding>& bindings,
    uint32_t bindlessCount)
{
    std::vector<const RHI::ShaderBinding*> expected;
    for (const auto& b : reflection.bindings)
        if (b.set == set)
            expected.push_back(&b);

    if (expected.size() != bindings.size()) {
        XConsole::PrintWarning("[Reflection] set {} binding count mismatch: layout has {}, shader signature has {}",
            set, bindings.size(), expected.size());
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < expected.size(); ++i) {
        const RHI::ShaderBinding& e = *expected[i];
        const RHI::RHIDescriptorBinding& b = bindings[i];
        const bool bindless = e.count == UINT32_MAX;
        if (e.binding != b.binding || e.type != b.type ||
            e.stage != b.stage || b.bindless != bindless ||
            (!bindless && e.count != b.count) ||
            (bindless && b.count != bindlessCount)) {
            XConsole::PrintWarning("[Reflection] set {} binding {} mismatch (layout vs shader signature)",
                set, b.binding);
            ok = false;
        }
    }
    return ok;
}

} // namespace Leir