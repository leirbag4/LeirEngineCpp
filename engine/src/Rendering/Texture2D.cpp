#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include <stb_image.h>

#include "LeirEngine/Core/Log.h"
#include <cstring>
#include <stdexcept>

namespace Leir {

void Texture2D::CreateFromData(const unsigned char* pixels, uint32_t width, uint32_t height,
    RHI::Filter filter, RHI::SamplerAddressMode addressMode)
{
    m_Width = width;
    m_Height = height;
    uint32_t rowPitch = width * 4;
    uint32_t align = m_Device->GetCopyRowPitchAlignment();
    if (align > 1) rowPitch = (rowPitch + align - 1) / align * align;
    uint32_t imageSize = rowPitch * height;

    RHI::RHIBuffer stagingBuffer;
    RHI::RHIDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(imageSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        stagingMemory);

    void* data;
    m_Device->MapMemory(stagingMemory, 0, imageSize, &data);
    if (align > 1 && rowPitch != width * 4) {
        // Pad each row so the D3D12 footprint's RowPitch alignment holds.
        for (uint32_t y = 0; y < height; ++y)
            memcpy((char*)data + (size_t)y * rowPitch,
                pixels + (size_t)y * width * 4, (size_t)width * 4);
    } else {
        memcpy(data, pixels, (size_t)imageSize);
    }
    m_Device->UnmapMemory(stagingMemory);

    m_Image = m_Device->CreateImage(width, height, RHI::Format::R8G8B8A8_SRGB,
        RHI::ImageUsage::TransferDst | RHI::ImageUsage::TransferSrc | RHI::ImageUsage::Sampled,
        RHI::MemoryProperty::DeviceLocal,
        m_Memory);

    m_Device->TransitionImageLayout(m_Image, RHI::Format::R8G8B8A8_SRGB,
        RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
    m_Device->CopyBufferToImage(stagingBuffer, m_Image, width, height);
    m_Device->TransitionImageLayout(m_Image, RHI::Format::R8G8B8A8_SRGB,
        RHI::ImageLayout::TransferDst, RHI::ImageLayout::ShaderReadOnly);

    m_Device->DestroyBuffer(stagingBuffer);
    m_Device->DestroyMemory(stagingMemory);

    m_ImageView = m_Device->CreateImageView(m_Image, RHI::Format::R8G8B8A8_SRGB, RHI::Aspect::Color);
    m_Sampler = m_Device->CreateSampler(filter, addressMode);

    // Register into the backend's global bindless table: the texture is then
    // referenced from shaders by m_BindlessIndex (no per-texture descriptor
    // set allocation).
    m_BindlessIndex = m_Device->RegisterBindlessTexture(GetDescriptorInfo());
}

Texture2D::Texture2D(RHI::RenderBackend* device, const std::string& path)
    : m_Device(device)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    static stbi_uc fallback[] = { 255, 0, 255, 255 };
    bool useFallback = false;

    if (!pixels) {
        XConsole::PrintError("Failed to load texture: {}", path);
        pixels = fallback;
        texWidth = texHeight = 1;
        texChannels = 4;
        useFallback = true;
    }

    CreateFromData(pixels, (uint32_t)texWidth, (uint32_t)texHeight);

    if (!useFallback)
        stbi_image_free(pixels);

    XConsole::Println("Texture loaded: {} ({}x{})", path, m_Width, m_Height);
}

Texture2D::Texture2D(RHI::RenderBackend* device, uint32_t width, uint32_t height,
                     const unsigned char* pixels)
    : m_Device(device)
{
    CreateFromData(pixels, width, height);
}

Texture2D::Texture2D(RHI::RenderBackend* device, Image& image, RHI::Filter filter, RHI::SamplerAddressMode addressMode)
    : m_Device(device)
{
    CreateFromData(image.GetData(), image.GetWidth(), image.GetHeight(), filter, addressMode);
}

Texture2D::~Texture2D()
{
    m_Device->UnregisterBindlessTexture(m_BindlessIndex);
    m_Device->DestroySampler(m_Sampler);
    m_Device->DestroyImageView(m_ImageView);
    m_Device->DestroyImage(m_Image);
    m_Device->DestroyMemory(m_Memory);
}

void Texture2D::UpdateFromImage(Image& image)
{
    uint32_t width = image.GetWidth();
    uint32_t height = image.GetHeight();
    uint32_t rowPitch = width * 4;
    uint32_t align = m_Device->GetCopyRowPitchAlignment();
    if (align > 1) rowPitch = (rowPitch + align - 1) / align * align;
    uint32_t imageSize = rowPitch * height;

    RHI::RHIBuffer stagingBuffer;
    RHI::RHIDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(imageSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        stagingMemory);

    void* data;
    m_Device->MapMemory(stagingMemory, 0, imageSize, &data);
    if (align > 1 && rowPitch != width * 4) {
        for (uint32_t y = 0; y < height; ++y)
            memcpy((char*)data + (size_t)y * rowPitch,
                image.GetData() + (size_t)y * width * 4, (size_t)width * 4);
    } else {
        memcpy(data, image.GetData(), (size_t)imageSize);
    }
    m_Device->UnmapMemory(stagingMemory);

    m_Device->TransitionImageLayout(m_Image, RHI::Format::R8G8B8A8_SRGB,
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::TransferDst);
    m_Device->CopyBufferToImage(stagingBuffer, m_Image, image.GetWidth(), image.GetHeight());
    m_Device->TransitionImageLayout(m_Image, RHI::Format::R8G8B8A8_SRGB,
        RHI::ImageLayout::TransferDst, RHI::ImageLayout::ShaderReadOnly);

    m_Device->DestroyBuffer(stagingBuffer);
    m_Device->DestroyMemory(stagingMemory);

    m_Width = image.GetWidth();
    m_Height = image.GetHeight();
}

} // namespace Leir
