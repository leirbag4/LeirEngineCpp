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
// Keyed by a hash of the path (+ content scale): repeated Load() calls return
// the cached Texture2D without re-decoding or re-registering in the bindless
// table. Entries are intentionally kept alive for the app lifetime (icons repeat
// a lot; it is a small fixed set). When contentScale > 1 the image is upscaled
// at load time (stb_image_resize2, linear) to native*scale texels so each texel
// maps 1:1 to a physical pixel at that DPI (same technique as the font atlas:
// crisp text/icons at any HiDPI). The caller keeps drawing the quad in logical
// units; only the texture resolution changes.
class LEIR_API UITextureCache {
public:
    static std::shared_ptr<Texture2D> Load(RHI::RenderBackend* device, const std::string& path,
        float contentScale = 1.0f);
    static void Clear();

private:
    static std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> s_Cache;
};

} // namespace Leir