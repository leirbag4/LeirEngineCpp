#include "LeirEngine/Rendering/Material.h"
#include "LeirEngine/Rendering/VulkanDevice.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/RenderPipeline.h"

#include <spdlog/spdlog.h>

namespace Leir {

Material::Material(VulkanDevice* device, std::shared_ptr<Shader> shader)
    : m_Device(device)
    , m_Shader(std::move(shader))
{
    CreateDescriptorPool();
    CreateDescriptorSetLayout();
    CreateDescriptorSet();
}

Material::~Material()
{
    auto dev = m_Device->GetDevice();
    if (m_Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(dev, m_Pipeline, nullptr);
    if (m_PipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(dev, m_PipelineLayout, nullptr);
    if (m_DescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(dev, m_DescriptorSetLayout, nullptr);
    if (m_UBOSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(dev, m_UBOSetLayout, nullptr);
    if (m_DescriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(dev, m_DescriptorPool, nullptr);
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture)
{
    (void)name;
    m_Texture = std::move(texture);
    UpdateDescriptorSet();
}

void Material::SetFloat(const std::string& name, float value)
{
    (void)name;
    (void)value;
}

void Material::SetVec3(const std::string& name, const Vector3& value)
{
    (void)name;
    (void)value;
}

void Material::Bind(VkCommandBuffer cmd, VkPipelineLayout layout) const
{
    (void)layout;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    if (m_DescriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineLayout, 1, 1, &m_DescriptorSet, 0, nullptr);
}

void Material::RecreatePipeline(VkRenderPass renderPass)
{
    auto dev = m_Device->GetDevice();
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    CreatePipeline(renderPass);
}

void Material::CreateDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };
    m_DescriptorPool = m_Device->CreateDescriptorPool(poolSizes, 1);
}

void Material::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_DescriptorSetLayout = m_Device->CreateDescriptorSetLayout({ samplerBinding });
}

void Material::CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
        spdlog::error("Failed to allocate material descriptor set");
}

void Material::UpdateDescriptorSet()
{
    if (!m_Texture)
        return;

    VkDescriptorImageInfo imageInfo = m_Texture->GetDescriptorInfo();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
}

void Material::CreatePipeline(VkRenderPass renderPass)
{
    auto dev = m_Device->GetDevice();

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    m_UBOSetLayout = m_Device->CreateDescriptorSetLayout({
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT }
    });

    VkDescriptorSetLayout setLayouts[] = {
        m_UBOSetLayout,
        m_DescriptorSetLayout
    };

    m_PipelineLayout = m_Device->CreatePipelineLayout(
        { setLayouts, setLayouts + 2 },
        { pushRange });

    auto binding = Vertex::GetBindingDescription();
    auto attrs = Vertex::GetAttributeDescriptions();

    m_Pipeline = m_Device->CreateGraphicsPipeline(
        m_PipelineLayout, renderPass,
        m_Shader->GetStageInfos(),
        binding, attrs);
}

VkDescriptorSetLayout Material::GetUBOSetLayout() const
{
    return m_UBOSetLayout;
}

} // namespace Leir
