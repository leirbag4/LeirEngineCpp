#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RHI.h"

#include <string>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }

class LEIR_API Shader {
public:
    Shader(RHI::RenderBackend* device,
           const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    size_t GetStageCount() const { return m_Stages.size(); }
    const std::vector<RHI::RHIShaderStageInfo>& GetStageInfos() const { return m_StageInfos; }

    // Hot-reload: re-read the bytecode files and recreate the shader modules.
    // Callers must also recreate any pipelines built from these stages.
    void Reload();

    static std::vector<char> ReadFile(const std::string& path);

private:
    void Load();

    RHI::RenderBackend* m_Device;
    std::string m_VertexPath;
    std::string m_FragmentPath;
    std::vector<RHI::ShaderStage> m_Stages;
    std::vector<RHI::RHIShaderModule> m_Modules;
    std::vector<RHI::RHIShaderStageInfo> m_StageInfos;
};

} // namespace Leir
