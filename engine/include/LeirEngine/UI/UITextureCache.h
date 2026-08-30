#pragma once

/**
 * @file UITextureCache.h
 * @brief Decode-once texture cache for file-loaded textures (icons, sprites).
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Leir {

namespace RHI { class RenderBackend; }
class Texture2D;

/**
 * @brief Decode-once texture cache keyed by path + content scale.
 * @ingroup UI
 * @details Repeated Load() calls return the cached Texture2D without re-decoding.
 *  Entries are kept alive for the app lifetime (small fixed set like icons).
 */
class LEIR_API UITextureCache {
public:
    /**
     * @brief Loads a texture from file (cached).
     * @param[in] device Render backend.
     * @param[in] path Path to image file.
     * @param[in] contentScale DPI scale for cache key.
     * @return Shared pointer to Texture2D (cached or newly decoded).
     */
    static std::shared_ptr<Texture2D> Load(RHI::RenderBackend* device, const std::string& path,
        float contentScale = 1.0f);

    /**
     * @brief Clears the cache (releases all cached textures).
     */
    static void Clear();

private:
    static std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> s_Cache; ///< Cache map.
};

} // namespace Leir
