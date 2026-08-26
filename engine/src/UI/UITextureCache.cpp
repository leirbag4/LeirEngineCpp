#include "LeirEngine/UI/UITextureCache.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/Core/Log.h"
#include "LeirEngine/Math/Vector4.h"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

// ============================================================================
// MODO DE CARGA DEL CACHE (elegí entre dos comportamientos) — LEIR_ICON_CPU_UPSCALE
// ----------------------------------------------------------------------------
//   0  = MODO LIGERO (DEFAULT, recomendado para TODO).  -> el que creó la IA
//        No se hace NINGÚN trabajo por-píxel en CPU. Se decodifica el PNG (eso
//        es inevitable, cualquier loader lo hace) y se sube tal cual a la GPU
//        con sampler Linear + ClampToEdge. El escalado a DPI > 100% queda 100%
//        en manos del sampler (bilinear). El margen y el antialiasing del borde
//        deben venir embebidos en el propio PNG (los iconos actuales los tienen).
//        O(1) en el peor caso por imagen: NO hay loop sobre los píxeles. Es
//        seguro para texturas de cualquier tamaño (incluso 4K), por eso es el
//        default: no hay forma de olvidarse y frenar el sistema con un loop.
//
//   1  = MODO NÍTIDO (HiDPI 1:1). -> el de la versión previa a la IA
//        Re-escala el contenido a native*scale texels con stb_image_resize2
//        (linear) para que cada texel mapee 1:1 a un píxel físico a esa DPI
//        (la misma técnica que usa el font atlas: texto/iconos nítidos). COSTO:
//        hace un resize por-píxel en CPU (bucle sobre TODA la imagen). Para
//        imágenes grandes esto realentiza la carga de forma notoria y aloca
//        memoria extra. Usá este modo SOLO para assets chicos (iconos <= 64px)
//        y solo si realmente notás el borde blando con el modo 0.
//
// Para comparar visualmente: cambiá el valor a 0/1, recompilá, y mirá los
// iconos en un monitor de 125% (el modo 1 los deja pixel-perfect; el 0 los
// deja un pelín más suaves pero con margen quedan bien). El modo 1 también
// emite un warning automático si detecta una imagen grande.
// ============================================================================
#define LEIR_ICON_CPU_UPSCALE 0

#if LEIR_ICON_CPU_UPSCALE
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include <stb_image_resize2.h>
#endif

namespace Leir {

std::unordered_map<uint64_t, std::shared_ptr<Texture2D>> UITextureCache::s_Cache;

// El umbral (px) a partir del cual el modo "nítido" (CPU-heavy) emite un warning:
// re-escalar una imagen más grande que esto recorre millones de píxeles por CPU
// y puede realentizar el arranque. Los iconos reales (~12-64px) están muy por
// debajo; las texturas de juego deberían pasar por la ruta de texturas normal.
static constexpr int kCpuUpscaleWarnSize = 256;

std::shared_ptr<Texture2D> UITextureCache::Load(RHI::RenderBackend* device, const std::string& path,
    float contentScale)
{
    if (!device || path.empty())
        return nullptr;
    if (contentScale < 1.0f)
        contentScale = 1.0f;

    // La key incluye contentScale aunque el modo 0 no lo use: garantiza que si
    // algún día se vuelve a activar LEIR_ICON_CPU_UPSCALE=1 las versiones por
    // escala no colisionen en la caché (y cubre el caso de mover la ventana a
    // otro monitor con distinta DPI).
    const uint64_t key = std::hash<std::string>{}(path + "|" + std::to_string(contentScale));
    auto it = s_Cache.find(key);
    if (it != s_Cache.end())
        return it->second;

    int w = 0, h = 0, srcChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &srcChannels, STBI_rgb_alpha);
    if (!pixels) {
        XConsole::PrintError("UITextureCache: cannot load '{}'", path);
        return nullptr;
    }

#if LEIR_ICON_CPU_UPSCALE
    // ---- Modo "nítido" (CPU-heavy): upscale por-píxel a native*scale ----
    // ADVERTENCIA: este branch recorre TODA la imagen en CPU (stb_image_resize2).
    // Para imágenes grandes puede realentizar la carga y duplicar la memoria
    // transitoria. El warning de abajo avisa; si lo ves, bajá este define a 0.
    if (w > kCpuUpscaleWarnSize || h > kCpuUpscaleWarnSize) {
        XConsole::PrintWarning(
            "UITextureCache: '{}' ({}x{} px) cargada con LEIR_ICON_CPU_UPSCALE=1 — el resize "
            "por CPU a native*scale ({:.2f}x -> {}x{} px) recorre cada píxel y puede "
            "realentizar la carga en imágenes grandes. Usá LEIR_ICON_CPU_UPSCALE=0 "
            "(modo ligero, sin loop por CPU) para texturas grandes.",
            path, w, h, contentScale, (int)std::lround((float)w * contentScale),
            (int)std::lround((float)h * contentScale));
    }

    const int cw = std::max(1, (int)std::lround((float)w * contentScale));
    const int ch = std::max(1, (int)std::lround((float)h * contentScale));
    std::vector<unsigned char> out((size_t)cw * ch * 4);
    stbir_resize_uint8_linear(pixels, w, h, 0, out.data(), cw, ch, 0, STBIR_RGBA);
    stbi_image_free(pixels);

    Image img((uint32_t)cw, (uint32_t)ch, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    std::memcpy(img.GetData(), out.data(), out.size());
    auto tex = std::make_shared<Texture2D>(device, img,
        RHI::Filter::Linear, RHI::SamplerAddressMode::ClampToEdge);
#else
    // ---- Modo "ligero" (DEFAULT): sin resize, sin loop por CPU ----
    // contentScale no participa: el sampler Linear escala al DPI. Se mantiene
    // el parámetro (y en la key) por compatibilidad de API y para el futuro.
    (void)contentScale;
    Image img((uint32_t)w, (uint32_t)h, Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    std::memcpy(img.GetData(), pixels, (size_t)w * h * 4);
    stbi_image_free(pixels);

    auto tex = std::make_shared<Texture2D>(device, img,
        RHI::Filter::Linear, RHI::SamplerAddressMode::ClampToEdge);
#endif

    s_Cache[key] = tex;
    return tex;
}

void UITextureCache::Clear()
{
    s_Cache.clear();
}

} // namespace Leir