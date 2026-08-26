#include "LeirEngine/UI/UITextureCache.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include <functional>

namespace Leir {

std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> UITextureCache::s_Cache;

std::shared_ptr<Texture2D> UITextureCache::Load(RHI::RenderBackend* device, const std::string& path)
{
    if (!device || path.empty())
        return nullptr;
    const uint64_t h = std::hash<std::string>{}(path);
    auto it = s_Cache.find(h);
    if (it != s_Cache.end())
        return it->second;
    // Texture2D(device, path) decodes with stb_image; a missing/bad file yields
    // the engine's magenta fallback (1x1), visible but identifiable as an error.
    auto tex = std::make_shared<Texture2D>(device, path);
    s_Cache[h] = tex;
    return tex;
}

void UITextureCache::Clear()
{
    s_Cache.clear();
}

} // namespace Leir