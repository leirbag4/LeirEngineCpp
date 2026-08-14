#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/IShaderCompiler.h"
#include "LeirEngine/RHI/RHI.h"

#include <string>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }

// Descriptor set layout derived from the shader reflection (Plan B, Fase 2).
// The `set` field keeps the binding space so the pipeline layout builder can
// order the layouts positionally (Vulkan pipeline-layout set order == set).
struct LEIR_API RHISetLayoutEntry {
    uint32_t set = 0;
    RHI::RHIDescriptorSetLayout layout;
};

// Load + merge the reflection sidecars for the given bytecode paths
// (".../Basic.vert.spv" / ".dxil" -> ".../Basic.vert.reflect.json"). The
// canonical sidecar JSON is produced by the editor shader tooling
// (ShaderExporter / ShaderHotReloader, see ShaderLayout.cpp for the format).
// Returns an empty reflection when a sidecar is missing so callers can fall
// back to hand-written layouts (engine works standalone, no compiler).
LEIR_API RHI::ShaderReflection LoadShaderReflectionFromSidecars(
    const std::vector<std::string>& bytecodePaths);

// One descriptor set layout per distinct set referenced by the reflection
// (ascending set order). Returns an empty vector for an empty reflection.
LEIR_API std::vector<RHISetLayoutEntry> CreateSetLayoutsFromReflection(
    RHI::RenderBackend* device, const RHI::ShaderReflection& reflection);

// Build the pipeline layout from pre-built set layouts + the reflection's
// push-constant ranges. Ranges with equal offset+size are merged into one
// range with combined stage flags (required by Vulkan for overlapping ranges).
// Every set layout is validated against the shader signature (debug). Returns
// an invalid handle for an empty reflection.
LEIR_API RHI::RHIPipelineLayout CreatePipelineLayoutFromReflection(
    RHI::RenderBackend* device, const RHI::ShaderReflection& reflection,
    const std::vector<RHISetLayoutEntry>& setLayouts);

// Debug-only: does `bindings` match the reflection's bindings for `set`?
// Logs a [Reflection] warning listing the mismatch and returns false.
LEIR_API bool ValidateSetLayoutAgainstReflection(
    const RHI::ShaderReflection& reflection, uint32_t set,
    const std::vector<RHI::RHIDescriptorBinding>& bindings);

} // namespace Leir