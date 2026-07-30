#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/Math/Mathf.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <spdlog/spdlog.h>
#include <cstring>

namespace Leir {

Image::Image(const std::string& path)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        spdlog::error("Image: failed to load {}", path);
        m_Width = 1;
        m_Height = 1;
        m_Data.resize(4, 255);
        return;
    }

    m_Width = (uint32_t)texWidth;
    m_Height = (uint32_t)texHeight;
    m_Channels = 4;
    m_Data.resize(m_Width * m_Height * 4);
    memcpy(m_Data.data(), pixels, m_Data.size());
    stbi_image_free(pixels);

    spdlog::info("Image loaded: {} ({}x{})", path, m_Width, m_Height);
}

Image::Image(uint32_t width, uint32_t height, const Vector4& color)
    : m_Width(width)
    , m_Height(height)
{
    m_Data.resize(width * height * 4);
    unsigned char r = (unsigned char)(Mathf::Clamp(color.x, 0.0f, 1.0f) * 255.0f);
    unsigned char g = (unsigned char)(Mathf::Clamp(color.y, 0.0f, 1.0f) * 255.0f);
    unsigned char b = (unsigned char)(Mathf::Clamp(color.z, 0.0f, 1.0f) * 255.0f);
    unsigned char a = (unsigned char)(Mathf::Clamp(color.w, 0.0f, 1.0f) * 255.0f);

    for (size_t i = 0; i < m_Data.size(); i += 4) {
        m_Data[i + 0] = r;
        m_Data[i + 1] = g;
        m_Data[i + 2] = b;
        m_Data[i + 3] = a;
    }
}

Vector4 Image::GetPixel(int x, int y) const
{
    if (x < 0 || x >= (int)m_Width || y < 0 || y >= (int)m_Height)
        return {0.0f, 0.0f, 0.0f, 1.0f};

    size_t idx = ((size_t)y * m_Width + (size_t)x) * 4;
    return {
        m_Data[idx + 0] / 255.0f,
        m_Data[idx + 1] / 255.0f,
        m_Data[idx + 2] / 255.0f,
        m_Data[idx + 3] / 255.0f
    };
}

void Image::SetPixel(int x, int y, const Vector4& color)
{
    if (x < 0 || x >= (int)m_Width || y < 0 || y >= (int)m_Height)
        return;

    size_t idx = ((size_t)y * m_Width + (size_t)x) * 4;
    m_Data[idx + 0] = (unsigned char)(Mathf::Clamp(color.x, 0.0f, 1.0f) * 255.0f);
    m_Data[idx + 1] = (unsigned char)(Mathf::Clamp(color.y, 0.0f, 1.0f) * 255.0f);
    m_Data[idx + 2] = (unsigned char)(Mathf::Clamp(color.z, 0.0f, 1.0f) * 255.0f);
    m_Data[idx + 3] = (unsigned char)(Mathf::Clamp(color.w, 0.0f, 1.0f) * 255.0f);
}

void Image::SavePNG(const std::string& path) const
{
    int result = stbi_write_png(path.c_str(), (int)m_Width, (int)m_Height, 4,
                                m_Data.data(), (int)m_Width * 4);
    if (result)
        spdlog::info("Image saved: {}", path);
    else
        spdlog::error("Image: failed to save {}", path);
}

std::unique_ptr<Image> Image::Clone() const
{
    auto clone = std::make_unique<Image>(m_Width, m_Height, Vector4{0.0f, 0.0f, 0.0f, 1.0f});
    clone->m_Data = m_Data;
    clone->m_Channels = m_Channels;
    return clone;
}

} // namespace Leir
