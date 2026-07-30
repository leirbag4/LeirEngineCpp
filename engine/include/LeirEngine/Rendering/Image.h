#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector4.h"
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class LEIR_API Image {
public:
    Image(const std::string& path);
    Image(uint32_t width, uint32_t height, const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});
    ~Image() = default;

    Vector4 GetPixel(int x, int y) const;
    void SetPixel(int x, int y, const Vector4& color);

    const unsigned char* GetData() const { return m_Data.data(); }
    unsigned char* GetData() { return m_Data.data(); }

    void SavePNG(const std::string& path) const;
    std::unique_ptr<Image> Clone() const;

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    int GetChannels() const { return m_Channels; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    int m_Channels = 4;
    std::vector<unsigned char> m_Data;
};

} // namespace Leir
