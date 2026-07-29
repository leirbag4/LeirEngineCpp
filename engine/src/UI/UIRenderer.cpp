#include "LeirEngine/UI/UIRenderer.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIImage.h"
#include "LeirEngine/UI/UISlider.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Rendering/VulkanDevice.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/Core/Settings.h"

#include <spdlog/spdlog.h>

namespace Leir {

UIRenderer::UIRenderer(VulkanDevice* device)
    : m_Device(device)
{
    auto dev = m_Device->GetDevice();

    // Descriptor set layout (binding 0: combined image sampler)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_DescSetLayout = m_Device->CreateDescriptorSetLayout({ binding });

    // Pool: up to 64 textures
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 }
    };
    m_DescPool = m_Device->CreateDescriptorPool(poolSizes, 64);

    // Pipeline layout
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::vec2);
    m_PipelineLayout = m_Device->CreatePipelineLayout({ m_DescSetLayout }, { pushRange });

    auto vertCode = Shader::ReadFile(LEIR_SHADER_DIR "/UI.vert.spv");
    auto fragCode = Shader::ReadFile(LEIR_SHADER_DIR "/UI.frag.spv");
    VkShaderModule vertMod = m_Device->CreateShaderModule(vertCode);
    VkShaderModule fragMod = m_Device->CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(UIVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs(3);
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(UIVertex, position);
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(UIVertex, texCoord);
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(UIVertex, color);

    m_Pipeline = m_Device->CreateGraphicsPipeline(
        m_PipelineLayout,
        m_Device->GetOverlayRenderPass(),
        { stages[0], stages[1] },
        bindingDesc, attrs,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        false,
        true);

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);

    // White fallback texture for untextured quads
    Image whiteImg(1, 1, glm::vec4(1.0f));
    m_FallbackTex = new Texture2D(m_Device, whiteImg);

    m_MaxVertices = 8192;
    VkDeviceSize vbSize = m_MaxVertices * sizeof(UIVertex);
    m_VertexBuffer = m_Device->CreateBuffer(vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_VertexMemory);

    spdlog::info("UIRenderer created");
}

UIRenderer::~UIRenderer()
{
    auto dev = m_Device->GetDevice();
    if (m_Pipeline) vkDestroyPipeline(dev, m_Pipeline, nullptr);
    if (m_PipelineLayout) vkDestroyPipelineLayout(dev, m_PipelineLayout, nullptr);
    if (m_DescSetLayout) vkDestroyDescriptorSetLayout(dev, m_DescSetLayout, nullptr);
    if (m_DescPool) vkDestroyDescriptorPool(dev, m_DescPool, nullptr);
    if (m_VertexBuffer) vkDestroyBuffer(dev, m_VertexBuffer, nullptr);
    if (m_VertexMemory) vkFreeMemory(dev, m_VertexMemory, nullptr);
    delete m_FallbackTex;
}

void UIRenderer::BuildBatch(Texture2D* texture, const glm::vec4& rect, const glm::vec4& uv, const glm::vec4& color)
{
    float x0 = rect.x;
    float y0 = rect.y;
    float x1 = rect.x + rect.z;
    float y1 = rect.y + rect.w;
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.z, v1 = uv.y + uv.w;

    m_Vertices.push_back({{x0, y0}, {u0, v0}, color});
    m_Vertices.push_back({{x1, y0}, {u1, v0}, color});
    m_Vertices.push_back({{x0, y1}, {u0, v1}, color});
    m_Vertices.push_back({{x1, y1}, {u1, v1}, color});

    m_QuadTextures.push_back(texture ? texture : m_FallbackTex);
}

void UIRenderer::Flush(VkCommandBuffer cmd)
{
    if (m_Vertices.empty()) return;

    VkDeviceSize vbSize = m_Vertices.size() * sizeof(UIVertex);
    if ((int)m_Vertices.size() > m_MaxVertices) {
        spdlog::warn("UIRenderer: overflow {} > {}", m_Vertices.size(), m_MaxVertices);
        m_Vertices.clear();
        m_QuadTextures.clear();
        return;
    }

    void* data;
    vkMapMemory(m_Device->GetDevice(), m_VertexMemory, 0, vbSize, 0, &data);
    memcpy(data, m_Vertices.data(), (size_t)vbSize);
    vkUnmapMemory(m_Device->GetDevice(), m_VertexMemory);

    VkExtent2D extent = m_Device->GetSwapchainExtent();
    glm::vec2 screenSize = {(float)extent.width, (float)extent.height};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    VkBuffer vb[] = { m_VertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec2), &screenSize);

    // Draw quad by quad for simplicity (no index buffer needed)
    for (size_t qi = 0; qi < m_QuadTextures.size(); ++qi) {
        Texture2D* tex = m_QuadTextures[qi];

        auto it = m_DescCache.find(tex);
        if (it == m_DescCache.end()) {
            VkDescriptorSet newSet;
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_DescPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &m_DescSetLayout;
            if (vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo, &newSet) != VK_SUCCESS) {
                spdlog::error("UIRenderer: failed to allocate desc set");
                continue;
            }
            VkDescriptorImageInfo imgInfo = tex->GetDescriptorInfo();
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = newSet;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &imgInfo;
            vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
            m_DescCache[tex] = newSet;
            it = m_DescCache.find(tex);
        }

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineLayout, 0, 1, &it->second, 0, nullptr);

        uint32_t vertOffset = (uint32_t)(qi * 4);
        vkCmdDraw(cmd, 4, 1, vertOffset, 0);
    }
}

void UIRenderer::Render(VkCommandBuffer cmd, UICanvas* canvas)
{
    m_Vertices.clear();
    m_QuadTextures.clear();

    if (!canvas || !canvas->IsActive()) return;

    // Walk tree depth-first
    std::vector<UIElement*> stack = { canvas };
    while (!stack.empty()) {
        UIElement* elem = stack.back();
        stack.pop_back();

        if (!elem->IsActive()) continue;
        const auto& cr = elem->GetComputedRect();

        if (auto* panel = dynamic_cast<UIPanel*>(elem)) {
            BuildBatch(nullptr, cr, {0, 0, 1, 1}, panel->GetColor());
        } else if (auto* img = dynamic_cast<UIImage*>(elem)) {
            Texture2D* tex = img->GetTexture();
            BuildBatch(tex, cr, {0, 0, 1, 1}, img->GetColor());
        } else if (auto* btn = dynamic_cast<UIButton*>(elem)) {
            // Background
            glm::vec4 bgColor;
            switch (btn->GetState()) {
                case ButtonState::Normal:  bgColor = btn->GetBgNormal(); break;
                case ButtonState::Hovered: bgColor = btn->GetBgHover(); break;
                case ButtonState::Pressed: bgColor = btn->GetBgPressed(); break;
            }
            BuildBatch(nullptr, cr, {0, 0, 1, 1}, bgColor);

            // Text
            if (btn->GetFont() && !btn->GetText().empty()) {
                float lineH = btn->GetFont()->GetLineHeight();
                float ascender = btn->GetFont()->GetAscender();
                float baselineY = cr.y + (cr.w - lineH) * 0.5f + ascender;
                auto rawQuads = btn->GetFont()->LayoutText(btn->GetText(), cr.z - 12.0f);
                for (size_t i = 0; i < rawQuads.size(); i += 2) {
                    const auto& r = rawQuads[i];
                    const auto& uv = rawQuads[i + 1];
                    glm::vec4 textRect = {cr.x + 6.0f + r.x, baselineY + r.y, r.z, r.w};
                    BuildBatch(btn->GetFont()->GetAtlasTexture(), textRect, uv, btn->GetTextColor());
                }
            }
        } else if (auto* slider = dynamic_cast<UISlider*>(elem)) {
            // Track background
            BuildBatch(nullptr, cr, {0, 0, 1, 1}, {0.2f, 0.2f, 0.2f, 1.0f});

            // Filled portion
            float handleT = slider->HandlePos();
            float fillW = cr.z * handleT;
            if (fillW > 0) {
                glm::vec4 fillRect = {cr.x, cr.y, fillW, cr.w};
                BuildBatch(nullptr, fillRect, {0, 0, 1, 1}, {0.4f, 0.6f, 1.0f, 1.0f});
            }

            // Handle
            float hx = cr.x + cr.z * handleT - 4.0f;
            glm::vec4 handleRect = {hx, cr.y - 2.0f, 8.0f, cr.w + 4.0f};
            BuildBatch(nullptr, handleRect, {0, 0, 1, 1}, slider->IsDragging()
                ? glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}
                : glm::vec4{0.8f, 0.8f, 0.8f, 1.0f});
        } else if (auto* label = dynamic_cast<UILabel*>(elem)) {
            if (label->GetFont() && label->GetFont()->GetAtlasTexture()) {
                for (const auto& gq : label->GetGlyphQuads()) {
                    glm::vec4 r = gq.rect;
                    r.x += cr.x;
                    r.y += cr.y;
                    BuildBatch(label->GetFont()->GetAtlasTexture(), r, gq.uv, gq.color);
                }
            }
        } else if (auto* input = dynamic_cast<UITextInput*>(elem)) {
            // Background
            BuildBatch(nullptr, cr, {0, 0, 1, 1}, {0.15f, 0.15f, 0.15f, 1.0f});

            // Text
            if (input->GetFont()) {
                std::string displayText = input->GetText().empty() ? input->GetPlaceholder() : input->GetText();
                glm::vec4 textColor = input->GetText().empty()
                    ? glm::vec4{0.5f, 0.5f, 0.5f, 1.0f}
                    : glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};

                float lineH = input->GetFont()->GetLineHeight();
                float ascender = input->GetFont()->GetAscender();
                float baselineY = cr.y + (cr.w - lineH) * 0.5f + ascender;
                auto rawQuads = input->GetFont()->LayoutText(displayText, cr.z - 8.0f);
                for (size_t i = 0; i < rawQuads.size(); i += 2) {
                    const auto& r = rawQuads[i];
                    const auto& uv = rawQuads[i + 1];
                    glm::vec4 textRect = {cr.x + 4.0f + r.x, baselineY + r.y, r.z, r.w};
                    BuildBatch(input->GetFont()->GetAtlasTexture(), textRect, uv, textColor);
                }
            }
        }

        if (LeirSettings::Get().debug.ui_outlines) {
            static const glm::vec4 debugOutlineColor = {0.0f, 1.0f, 0.0f, 1.0f};
            float t = 2.0f;
            BuildBatch(nullptr, {cr.x, cr.y, cr.z, t}, {0,0,1,1}, debugOutlineColor);
            BuildBatch(nullptr, {cr.x, cr.y + cr.w - t, cr.z, t}, {0,0,1,1}, debugOutlineColor);
            BuildBatch(nullptr, {cr.x, cr.y, t, cr.w}, {0,0,1,1}, debugOutlineColor);
            BuildBatch(nullptr, {cr.x + cr.z - t, cr.y, t, cr.w}, {0,0,1,1}, debugOutlineColor);
        }

        // Children (depth-first: push children in reverse so first child is drawn first)
        for (auto it = elem->GetChildren().rbegin(); it != elem->GetChildren().rend(); ++it) {
            stack.push_back(*it);
        }
    }

    Flush(cmd);
}

} // namespace Leir
