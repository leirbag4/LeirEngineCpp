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

### Hallazgos del build (verificados 2026-08-17, investigación M4)
- **SoLoud NO se compila hoy en ningún lado.** El repo `jarikomppa/soloud`
  **no tiene `CMakeLists.txt` en la raíz** → `FetchContent_MakeAvailable(soloud)`
  solo lo *puebla* (no crea target) → el `if(TARGET soloud)` de
  `engine/CMakeLists.txt:228` es **falso** y nunca se linkea. Es dead weight
  (costo de fetch). Commit fetcheado actual: `e82fd32c` (`GIT_TAG master`).
- **SoLoud master es thread-free desde el lado del motor**: `soloud.cpp` NO
  llama `createThread` (el SoLoud viejo tenía un thread interno; el actual
  corre el mix desde el callback del backend — `soloud_miniaudio_audiomixer`
  llama `soloud->mix()` directo). En web el callback lo dispara WebAudio vía JS.
  Solo usa mutexes (`soloud_thread.cpp`), que en Emscripten sin `-pthread` son
  stubs que linkean y no hacen nada (single-thread) → **no choca con la decisión
  single-threaded de Fase 6**.
- **El CMake de `contrib/` (comunitario) NO sirve tal cual**: compila TODOS los
  audiosources (ay/monotone/openmpt/sfxr/speech/tedsid/vic/vizsn + todos los
  filters + C API; `openmpt` exige libopenmpt → riesgo de link) y **no cubre
  miniaudio** (solo NULL/SDL2/ALSA/COREAUDIO/OPENSLES/XAUDIO2/WINMM/WASAPI).
  → **Target propio**: snippet CMake (patrón `cmake/SlangTooling.cmake`) con solo:
  - **core** (19 fuentes de `src/core/`, incl. `soloud_thread.cpp`)
  - **wav** (`src/audiosource/wav/`): `soloud_wav.cpp`, `soloud_wavstream.cpp`
    (WAV/MP3/FLAC vía `dr_impl.cpp` + OGG vía `stb_vorbis.c`, ambos vendored en
    soloud; confirmado por los includes de `soloud_wav.cpp:31-34`)
  - **backend desktop Windows**: `soloud_wasapi.cpp` (WASAPI; link `ole32`+`avrt`)
  - **backend Linux/macOS/CI**: `soloud_null.cpp` (no hay device de audio en CI)
  - **backend web**: `soloud_miniaudio.cpp` (miniaudio.h vendored en soloud)
  - filtros: omitir en M4 (no hacen falta para el demo).
- **Web = miniaudio → WebAudio** (confirmado en miniaudio.h): `MA_EMSCRIPTEN` se
  auto-detecta; emite JS directo (`EM_ASM`): `new AudioContext` (creado
  `suspend()`), `ma_device_start` → `resume()`, `ma_device_stop` → `suspend()`
  (líneas 32273-32561).
- **Constraint crítica del backend web**: miniaudio.h:276 y 1466 — *"You cannot
  use `-std=c*` compiler flags, nor `-ansi`"* (solo build Emscripten). Nuestro
  web build fuerza `-std=c++20` (`CMAKE_CXX_EXTENSIONS OFF`) → el target `soloud`
  web se compila con **`-std=gnu++20`** (o sin `-std`).
- **Autoplay policy**: el `AudioContext` nace suspendido y el browser exige un
  **gesto del usuario** para `resume()`. → init del audio **lazy en el primer
  gesto** (primer `OnPointerDown`/click) o `resume()` en el gesto. El WebEngineDemo
  hoy no maneja gestos (UI estática) → agregar un click-to-enable-audio.
- **Pinning**: `GIT_TAG master` es target móvil → pinear SoLoud a `e82fd32c`
  (el commit ya fetcheado) para reproducibilidad (patrón Jolt `2e28006`).
- **Assets**: WebEngineDemo ya preloads `assets@/assets` → solo agregar
  `assets/audio/` (WAV/OGG).
- Desktop: el `if(TARGET soloud)` ya está listo en `engine/CMakeLists.txt:228`;
  agregar el target en `dependencies/CMakeLists.txt` (tras MakeAvailable) o en un
  include compartido (reusable en el web demo).

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

### Target `soloud` propio (ambas plataformas)
- Snippet CMake reutilizable (patrón `cmake/SlangTooling.cmake`) que crea
  `add_library(soloud STATIC <core+wav+backend>)` desde `${soloud_SOURCE_DIR}`:
  - core 19 fuentes + `soloud_wav.cpp`/`soloud_wavstream.cpp`/`dr_impl.cpp`/`stb_vorbis.c`
  - backend por plataforma: `wasapi` (Windows desktop), `null` (Linux/macOS/CI),
    `miniaudio` (`__EMSCRIPTEN__`)
  - Windows: link `PRIVATE ole32 avrt`; web: compilar con `-std=gnu++20`
    (constraint miniaudio); CI Linux/macOS: `null` para no depender de device.
- Pin SoLoud a commit `e82fd32c`.

### Desktop (`dependencies/CMakeLists.txt` + `engine/CMakeLists.txt`)
- Crear el target `soloud` tras `FetchContent_MakeAvailable` → el
  `if(TARGET soloud)` de `engine/CMakeLists.txt:228` ya linkea.
- Agregar a `target_sources` del engine: `src/Audio/AudioBackend.cpp`,
  `src/Audio/SoLoudBackend.cpp`, `src/Audio/AudioEngine.cpp`,
  `src/Components/AudioSource.cpp`, `src/Components/AudioListener.cpp`.

### Web (`examples/WebEngineDemo/CMakeLists.txt` + `engine/CMakeLists.web.txt`)
- Agregar fetch de SoLoud (mismo patrón que Jolt) + crear el target `soloud` con
  backend `miniaudio` + `-std=gnu++20`.
- Agregar las fuentes de audio a `LeirEngineCore` + link `PRIVATE soloud`.
- `--preload-file` de `assets@/assets` ya cubre `assets/audio/`.

## Gotchas (estado tras la investigación 2026-08-17)

1. **Autoplay policy del browser** (PENDIENTE de resolver en implementación): el
   `AudioContext` nace suspendido y el browser exige un **gesto del usuario** para
   `resume()`. Plan: **init lazy del audio en el primer gesto** (primer
   `OnPointerDown`/click del demo) — el WebEngineDemo no maneja gestos hoy, hay que
   agregar click-to-enable-audio.
2. **miniaudio en Emscripten**: NO compila con `-std=c*` estricto ni `-ansi`
   (miniaudio.h:276,1466) → compilar el TU del backend miniaudio con
   `-std=gnu++20`. ✅ Verificado contra miniaudio.h; falta verificar contra emsdk 6.0.6.
3. **Pin de SoLoud**: `GIT_TAG master` → pinear a commit **`e82fd32c`** (el ya
   fetcheado). ✅ Decidido.
4. **Streaming en web**: todo el asset vive en memoria (preload) — `WavStream`
   funciona sobre el archivo preloadado; no hay streaming real por red en el demo.
5. **Desktop NO compila SoLoud hoy** (sin CMakeLists en el repo → sin target) —
   el build de audio arranca de cero con el target propio. ✅ Diagnóstico hecho.

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
1. ~~Spike~~ investigación de SoLoud/build hecha (2026-08-17) — queda el spike de
   build: target `soloud` propio (wasapi/null/miniaudio) + validar miniaudio contra
   emsdk 6.0.6 (`-std=gnu++20`) + autoplay con init lazy.
2. `IAudioBackend` + `SoLoudBackend` + `AudioEngine` (desktop).
3. `AudioSource`/`AudioListener` + sync 3D en `Scene::OnUpdate`.
4. Assets + integración builds (desktop + web).
5. Demo con música + SFX + verificación Firefox y desktop.
6. Docs + CI + commit (sin `[skip ci]`).
