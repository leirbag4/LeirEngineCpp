#pragma once
#include "LeirEngine/Core/Export.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Leir {

namespace RHI { class RenderBackend; }
class Texture2D;

// Decode-once texture cache for file-loaded textures (icons, sprites, ...).
// Keyed by a hash of the path + content scale: repeated Load() calls return
// the cached Texture2D without re-decoding or re-registering in the bindless
// table. Entries are intentionally kept alive for the app lifetime (icons repeat
// a lot; it is a small fixed set).
//
// Loading has two modes, selected by the LEIR_ICON_CPU_UPSCALE define in
// UITextureCache.cpp:
//   * 0 (DEFAULT, light): the PNG is uploaded as-is (Linear + ClampToEdge) with
//     no per-pixel CPU work; DPI scaling is left to the GPU sampler. Margin and
//     antialiasing must be baked into the PNG. Safe for images of any size.
//   * 1 (crisp): the content is upscaled to native*scale texels at load time
//     (stb_image_resize2, linear) so each texel maps 1:1 to a physical pixel at
//     that DPI (same technique as the font atlas). Per-pixel CPU loop; use only
//     for small assets (icons) and it warns for images larger than 256px.
// contentScale is always part of the cache key so both modes stay distinct.
class LEIR_API UITextureCache {
public:
    static std::shared_ptr<Texture2D> Load(RHI::RenderBackend* device, const std::string& path,
        float contentScale = 1.0f);
    static void Clear();

private:
    static std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> s_Cache;
};

} // namespace Leir