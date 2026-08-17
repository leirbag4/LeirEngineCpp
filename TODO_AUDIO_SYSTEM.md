# TODO_AUDIO_SYSTEM.md — Fase 4: Audio (SoLoud → WebAudio)

> Estado: **PENDIENTE / DIFERIDO** (planificado 2026-08-17, sin fecha). El motor
> NO tiene subsistema de audio: SoLoud está fetcheado y linkeado pero **no se usa
> en ningún lado**. Nunca se reprodujo música ni SFX en el proyecto. Este doc es el
> plan completo para implementarlo (desktop + web).

## Objetivo

Construir el **subsistema de audio del motor** con una **abstracción propia**
(independiente de la biblioteca), implementado sobre **SoLoud** en desktop y
**WebAudio** en navegador, más los componentes `AudioSource`/`AudioListener` que
la jerarquía de AGENTS.md ya promete.

## Contexto técnico (verificado 2026-08-17)

### Qué es SoLoud
- Biblioteca de audio **C++** (Jari Komppa, licencia **zlib**) orientada a juegos:
  mezcla, carga WAV/OGG/MP3/FLAC, streaming (`WavStream`), loops, volumen/pitch,
  **audio 3D espacializado**, filtros (lowpass/highpass/echo/FFT), varios backends
  de salida por plataforma.

### Qué es WebAudio
- **API de audio del navegador** (JavaScript): grafo de nodos (`AudioContext`,
  `GainNode`, `AudioBufferSourceNode`, …). Es el único "sonido" en el browser.
  Al compilar C++→wasm no hay tarjeta de sonido: el audio wasm termina en
  WebAudio vía JS glue.

### Multiplataforma
- SoLoud: desktop (Windows WASAPI/DirectSound/WinMM, macOS CoreAudio, Linux
  ALSA/Pulse) y **web vía backend `miniaudio`**, que en Emscripten usa
  `ma_backend_webaudio`.
- **NO hay backend `emscripten` dedicado en el SoLoud actual** (master) — el
  camino web es `src/backend/miniaudio/` (`soloud_miniaudio.cpp` + `miniaudio.h`
  vendored; confirmado `ma_backend_webaudio` en miniaudio.h:1422, soporte
  Emscripten en miniaudio.h:274-276).

### Estado actual de la integración en el motor (NO funciona)
- `dependencies/CMakeLists.txt:43-45` — SoLoud se fetchea (`GIT_TAG master`) y se
  compila en cada build = **dead weight**.
- `engine/CMakeLists.txt:227-230` — `LeirEngine` lo linkea `PRIVATE` (`if(TARGET soloud)`).
- **Cero código de audio**: no existe `engine/src/Audio/`, no existen
  `Components/AudioSource.h`/`AudioListener.h` (los únicos componentes reales son
  Camera/Light/MeshRenderer/SpriteRenderer), y ningún `.cpp` referencia SoLoud.
- La jerarquía de AGENTS.md *promete* `AudioSource`/`AudioListener` — nunca se
  implementaron. Por eso nunca hubo música ni SFX.
- No existe **ninguna** abstracción propia de audio (como sí la hay para RHI y
  física).

## Arquitectura propuesta (patrón RHI/Physics, biblioteca aislada)

Regla: **los headers públicos NUNCA exponen tipos de SoLoud** (como
`PhysicsConversions.h` para Jolt). Todo handle = `uint32_t`. El usuario del motor
usa **métodos propios**: `.Play()`, `.Stop()`, `.Pause()`, `.Resume()`, `.Seek()`,
`.SetVolume()`, `.SetPitch()`, `.SetLooping()`, etc.

```
engine/include/LeirEngine/Audio/
├── AudioBackend.h    → interfaz IAudioBackend (LEIR_API)
├── AudioEngine.h     → singleton (como PhysicsWorld/SceneManager)
└── AudioClip.h       → asset de audio (datos crudos, formato-neutral)

engine/src/Audio/
├── AudioBackend.cpp
├── SoLoudBackend.cpp → implementación real (SoLoud::Soloud + Wav/WavStream)
└── AudioEngine.cpp

engine/include/LeirEngine/Components/
├── AudioSource.h
└── AudioListener.h
engine/src/Components/AudioSource.cpp
engine/src/Components/AudioListener.cpp
```

### Interfaz propuesta — IAudioBackend (métodos propios del motor)
```cpp
// Todos los handles son uint32_t opacos. Sin tipos SoLoud en público.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    virtual bool Init() = 0;                  // abre el device de salida
    virtual void Shutdown() = 0;              // cierra device, libera todo
    virtual void Update(float dt) = 0;        // sync 3D, procesos de fondo

    // Clips (assets)
    virtual uint32_t LoadClip(const std::string& path) = 0;   // devuelve handle
    virtual void UnloadClip(uint32_t clip) = 0;

    // Sources (instancias reproducibles)
    virtual uint32_t CreateSource() = 0;      // source vacío
    virtual void DestroySource(uint32_t src) = 0;
    virtual void Play(uint32_t src, uint32_t clip) = 0;   // clip opcional
    virtual void Stop(uint32_t src) = 0;
    virtual void Pause(uint32_t src) = 0;
    virtual void Resume(uint32_t src) = 0;
    virtual void Seek(uint32_t src, double seconds) = 0;
    virtual void SetLooping(uint32_t src, bool loop) = 0;
    virtual void SetVolume(uint32_t src, float v) = 0;
    virtual void SetPitch(uint32_t src, float pitch) = 0;
    virtual void SetSource3D(uint32_t src, float x, float y, float z) = 0;

    // Mezcla global
    virtual void SetMasterVolume(float v) = 0;
    virtual float GetMasterVolume() const = 0;

    // Listener (audio 3D)
    virtual void SetListener3D(const glm::vec3& pos,
                               const glm::vec3& fwd,
                               const glm::vec3& up) = 0;
    virtual bool Supports3D() const = 0;
};
```

### SoLoudBackend (implementación)
- Mapea la interfaz a `SoLoud::Soloud` + `SoLoud::Wav`/`WavStream` + `SoLoud::handle`.
- En **desktop**: backend de salida WASAPI/WinMM/DSound (SoLoud lo elige solo).
- En **web (`__EMSCRIPTEN__`)**: backend `miniaudio` → WebAudio (`ma_backend_webaudio`).
- `AudioSource::OnDestroy` libera su handle; `AudioEngine::Shutdown` limpia todo.

### AudioEngine (singleton)
```cpp
class AudioEngine {
public:
    static AudioEngine& GetInstance();
    void Init();                // crea el IAudioBackend según plataforma
    void Shutdown();
    void Update(float dt);      // lo llama Scene::OnUpdate
    IAudioBackend* GetBackend();
};
```

### Componentes
- **AudioSource**: `Play()/Stop()/Pause()/Resume()/Seek()/SetLooping()/SetVolume()/
  SetPitch()/SetClip()`, flag `Spatial3D` → posición desde el `Transform` sincronizada
  en `Scene::OnUpdate`. Destructor/`OnDestroy` → `DestroySource`.
- **AudioListener**: pos/orientación (mundo) → `SetListener3D`. La cámara primaria
  alimenta al listener (o un AudioListener explícito en la escena).

## Assets (plan)
- Commitear 2 archivos **CC0** en `assets/audio/` (como `assets/Roboto-Regular.ttf`):
  un SFX corto (WAV, p. ej. beep/impacto) + una música corta (OGG loop).
- Generar con ffmpeg o bajar de fuente libre. SoLoud carga WAV/OGG (stb_vorbis
  incluido). Música larga → `WavStream` (streaming).
- Web: `--preload-file assets@/assets`.

## Integración de builds

### Desktop (`engine/CMakeLists.txt`)
- Agregar a `target_sources`: `src/Audio/AudioBackend.cpp`,
  `src/Audio/SoLoudBackend.cpp`, `src/Audio/AudioEngine.cpp`,
  `src/Components/AudioSource.cpp`, `src/Components/AudioListener.cpp`.
- SoLoud ya está linkeado (`PRIVATE`) — no tocar nada más.

### Web (`engine/CMakeLists.web.txt`)
- Agregar fetch de SoLoud (mismo patrón que Jolt en el web build) + las mismas
  fuentes de audio + link. Backend = `miniaudio`.

## Gotchas conocidos a resolver (spike previo)

1. **Autoplay policy del browser**: el `AudioContext` NO arranca sin un gesto del
   usuario → inicializar/despertar el audio en el **primer click** (botón "Enable
   Audio" o init en el primer `OnPointerDown`). En SoLoud: init diferido o
   `setPause`/`setVolume(0)` + resume en el gesto.
2. **miniaudio en Emscripten**: no compila con `-std=c*` estricto (usar dialecto
   gnu, p. ej. `-std=gnu++20`) en el TU del backend. Verificar con emsdk 6.0.6.
3. **Pin de SoLoud**: `GIT_TAG master` es un target móvil; evaluar pin a un commit
   fijo para reproducibilidad (como Jolt `2e28006` en web).
4. **Streaming en web**: todo el asset vive en memoria (preload) — `WavStream`
   funciona sobre el archivo preloadado; no hay streaming real por red en el demo.

## Verificación / Demo

- **WebEngineDemo** (recomendado, ya es el demo "motor completo"): música de fondo
  loop + SFX al hacer click (+ opcional beep al impactar los cubos con el piso).
  Verificación: **Firefox** (web) + demo/editor nativo desktop.
- Alternativa: demo `AudioDemo` aparte.

## Docs / CI

- AGENTS.md: sección "Audio System" (arquitectura, SoLoud→WebAudio, autoplay) +
  entrada en `## Previous Changes Summary`.
- TODO_WEB_EXPORT.md: actualizar el riesgo "SoLoud bajo Emscripten (M4)" cuando
  se implemente.
- Commit **sin** `[skip ci]` → CI valida 3 plataformas + job emscripten.

## Decisiones pendientes (usuario, cuando retomemos)
1. **Audio 3D**: ¿completo (espacializado, listener con posición — recomendado, lo
   promete la jerarquía) o solo 2D (volumen/pitch)?
2. **Demo**: ¿integrar en `WebEngineDemo` (recomendado) o `AudioDemo` aparte?
3. **Assets**: ¿generar WAV+OGG CC0 de prueba o archivos propios del usuario?

## Fases sugeridas (cuando se retome)
1. Spike: SoLoud+miniaudio en Emscripten (flags, autoplay) + SoLoud desktop.
2. `IAudioBackend` + `SoLoudBackend` + `AudioEngine` (desktop).
3. `AudioSource`/`AudioListener` + sync 3D en `Scene::OnUpdate`.
4. Assets + integración builds (desktop + web).
5. Demo con música + SFX + verificación Firefox y desktop.
6. Docs + CI + commit.
