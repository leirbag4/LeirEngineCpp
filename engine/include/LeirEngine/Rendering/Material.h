#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/RHI/RHI.h"

#include <memory>
#include <string>

namespace Leir {

namespace RHI { class RenderBackend; }
class Texture2D;
class Shader;

class LEIR_API Material {
public:
    Material(RHI::RenderBackend* device, std::shared_ptr<Shader> shader);
    ~Material();

    void SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture);
    void SetColor(const Vector4& color) { m_Color = color; }
    Vector4 GetColor() const { return m_Color; }
    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const Vector3& value);

    void Bind(RHI::RHICommandBuffer cmd, RHI::RHIPipelineLayout layout) const;

    RHI::RHIPipeline GetPipeline() const { return m_Pipeline; }
    RHI::RHIPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    RHI::RHIDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
    std::shared_ptr<Shader> GetShader() const { return m_Shader; }
    RHI::RHIDescriptorSetLayout GetUBOSetLayout() const;

    // Recreate pipeline when device is lost
    void RecreatePipeline(RHI::RHIRenderPass renderPass);

    // Shader hot-reload: recreates only the graphics pipeline from the shader's
    // current stages. Descriptor/pipeline layouts are kept (bindings are
    // assumed unchanged by a reload — see TODO_RHI_SLANG.md Plan A).
    void ReloadShaders(RHI::RHIRenderPass renderPass);

private:
    void CreateDescriptorPool();
    void CreateDescriptorSetLayout();
    void CreateDescriptorSet();
    void UpdateDescriptorSet();
    void CreatePipeline(RHI::RHIRenderPass renderPass);
    void BuildPipeline(RHI::RHIRenderPass renderPass);

    RHI::RenderBackend* m_Device;
    std::shared_ptr<Shader> m_Shader;
    Vector4 m_Color{1.0f, 1.0f, 1.0f, 1.0f};

    RHI::RHIDescriptorPool m_DescriptorPool;
    RHI::RHIDescriptorSetLayout m_DescriptorSetLayout;
    RHI::RHIDescriptorSetLayout m_UBOSetLayout;
    RHI::RHIDescriptorSet m_DescriptorSet;

    RHI::RHIPipelineLayout m_PipelineLayout;
    RHI::RHIPipeline m_Pipeline;

    std::shared_ptr<Texture2D> m_Texture;
};

} // namespace Leir
