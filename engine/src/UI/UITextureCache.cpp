#include "LeirEngine/UI/UITextureCache.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/Core/Log.h"
#include "LeirEngine/Math/Vector4.h"

#include <stb_image.h>

#include <cstring>
#include <functional>

namespace Leir {

std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> UITextureCache::s_Cache;

// Sin resize ni padding sintético por CPU: se asume que 'path' ya viene con
// margen transparente real agregado en el propio PNG (como hiciste vos).
// Se decodifica el archivo (eso es inevitable, cualquier loader lo hace) y se
// sube tal cual a la GPU. El anti-aliasing en el borde a distintos DPI queda
// 100% en manos del sampler (Filter::Linear + ClampToEdge).
std::shared_ptr<Texture2D> UITextureCache::Load(RHI::RenderBackend* device, const std::string& path,
    float contentScale)
{
    if (!device || path.empty())
        return nullptr;

    // contentScale ya no participa en ningún resize: se ignora a propósito.
    // Se deja el parámetro para no romper las firmas de los callers existentes.
    (void)contentScale;

    const uint64_t key = std::hash<std::string>{}(path);
    auto it = s_Cache.find(key);
    if (it != s_Cache.end())
        return it->second;

    int w = 0, h = 0, srcChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &srcChannels, STBI_rgb_alpha);
    if (!pixels) {
        XConsole::PrintError("UITextureCache: cannot load '{}'", path);
        return nullptr;
    }

    Image img((uint32_t)w, (uint32_t)h, Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    std::memcpy(img.GetData(), pixels, (size_t)w * h * 4);
    stbi_image_free(pixels);

    auto tex = std::make_shared<Texture2D>(device, img,
        RHI::Filter::Linear, RHI::SamplerAddressMode::ClampToEdge);

    s_Cache[key] = tex;
    return tex;
}

void UITextureCache::Clear()
{
    s_Cache.clear();
}

} // namespace Leir