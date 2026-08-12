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

    static std::vector<char> ReadFile(const std::string& path);

private:
    RHI::RenderBackend* m_Device;
    std::vector<RHI::ShaderStage> m_Stages;
    std::vector<RHI::RHIShaderModule> m_Modules;
    std::vector<RHI::RHIShaderStageInfo> m_StageInfos;
};

} // namespace Leir
