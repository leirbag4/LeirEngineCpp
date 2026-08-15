#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/RHI/RHI.h"

#include <string>

namespace Leir {

namespace RHI { class RenderBackend; }

class LEIR_API Texture2D {
public:
    Texture2D(RHI::RenderBackend* device, const std::string& path);
    Texture2D(RHI::RenderBackend* device, uint32_t width, uint32_t height,
              const unsigned char* pixels);
    Texture2D(RHI::RenderBackend* device, Image& image,
              RHI::Filter filter = RHI::Filter::Linear,
              RHI::SamplerAddressMode addressMode = RHI::SamplerAddressMode::Repeat);
    ~Texture2D();

    void UpdateFromImage(Image& image);

    RHI::RHIImageView GetImageView() const { return m_ImageView; }
    RHI::RHISampler GetSampler() const { return m_Sampler; }
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    // Stable index into the backend's global bindless texture table.
    uint32_t GetBindlessIndex() const { return m_BindlessIndex; }

    RHI::RHIDescriptorImageInfo GetDescriptorInfo() const {
        RHI::RHIDescriptorImageInfo info;
        info.imageView = m_ImageView;
        info.sampler = m_Sampler;
        info.image = m_Image;
        info.valid = true;
        return info;
    }

private:
    void CreateFromData(const unsigned char* pixels, uint32_t width, uint32_t height,
        RHI::Filter filter = RHI::Filter::Linear,
        RHI::SamplerAddressMode addressMode = RHI::SamplerAddressMode::Repeat);

    RHI::RenderBackend* m_Device;
    RHI::RHIImage m_Image;
    RHI::RHIDeviceMemory m_Memory;
    RHI::RHIImageView m_ImageView;
    RHI::RHISampler m_Sampler;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    uint32_t m_BindlessIndex = 0;
};

} // namespace Leir
