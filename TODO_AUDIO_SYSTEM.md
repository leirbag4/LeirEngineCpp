# TODO_AUDIO_SYSTEM.md — Fase 4: Audio (SoLoud → WebAudio)

> Estado: **COMPLETO** (2026-08-17, verificado en Firefox por el usuario:
> música loop + beep 2D + pop 3D con click). Diseño final cerrado con el usuario
> (audio 3D completo, `SoundPlayer` estático con métodos cortos, `volume` como
> nombre unificado, 8 overloads de `Play` con vec3). Implementado: target
> `soloud` propio + `IAudioBackend`/`AudioEngine`/`AudioClip`/`AudioSource`/
> `AudioListener`/`SoundPlayer` + demo en WebEngineDemo + assets WAV/OGG CC0 +
> builds desktop y web verdes. Nota: la demo web necesitó exportar 3 símbolos de
> miniaudio al módulo JS de Emscripten (ver "Gotchas" — `_malloc`/`HEAPF32`/`ccall`).

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

## Diseño final (cerrado con el usuario, 2026-08-17)

### Tabla de nombres por motor (Unity / Unreal / Leir)

| Concepto | Unity | Unreal | **Leir (nuestro)** |
|---|---|---|---|
| Singleton de audio | `AudioSettings` / `AudioMixer` | `FAudioDevice` / `UAudioSubsystem` | `AudioEngine` |
| Clip / asset | `AudioClip` | `USoundBase` (`USoundWave`/`USoundCue`) | `AudioClip` |
| Emisor 3D (componente) | `AudioSource` | `UAudioComponent` | `AudioSource` |
| Oyente | `AudioListener` | (listener del player) | `AudioListener` |
| Quick-play estático | `AudioSource.PlayOneShot(clip)` | `UGameplayStatics::PlaySound2D`/`PlaySoundAtLocation` | `SoundPlayer` (estáticos) |
| Handle por fuente | `AudioSource` | `FActiveSound` | `SoundId` (uint32 opaco) |
| Estado | `isPlaying` | `EAudioComponentPlayState` (`Playing/Stopped/Paused/Loading`) | `SoundState` |
| Volumen | `volume` | `SetVolumeMultiplier` | `SetVolume` |
| Pitch | `pitch` | `SetPitchMultiplier` | `SetPitch` |
| Loop | `loop` | `bLooping` | `SetLooping` / `loop` |
| Seek / tiempo | `AudioSource.time` | (limitado) | `Seek(seconds)` / `GetTime` |
| Posición 3D | `transform.position` | `PlaySoundAtLocation(Location)` | `Play(..., pos)` / `SetPosition(id, pos)` |
| Falloff 3D | `minDistance`/`maxDistance` | `USoundAttenuation` | `SetMinDistance`/`SetMaxDistance` |
| Mute/pausa global | `AudioListener.pause` | `FAudioDevice` | `SetMasterVolume` / `PauseAll` |

### Arquitectura (patrón RHI/Physics, biblioteca aislada)

Regla: **los headers públicos NUNCA exponen tipos de SoLoud** (como
`PhysicsConversions.h` para Jolt). Todo handle = `SoundId`/`ClipId` (`uint32_t`).
Las posiciones usan `Leir::Vector3` (el `Transform` del engine usa esos tipos, no glm).

```
engine/include/LeirEngine/Audio/
├── AudioTypes.h      → SoundId, ClipId, SoundState
├── AudioBackend.h    → interfaz IAudioBackend (LEIR_API)
├── AudioEngine.h     → singleton + caché de clips (patrón PhysicsWorld)
├── AudioClip.h       → asset de audio (path + duration + ClipId)
└── SoundPlayer.h     → API estática rápida (patrón XConsole/Keyboard)

engine/src/Audio/
├── AudioEngine.cpp
├── AudioClip.cpp
├── SoLoudBackend.cpp → implementación real (SoLoud::Soloud + Wav)
└── SoundPlayer.cpp

engine/include/LeirEngine/Components/
├── AudioSource.h
└── AudioListener.h
engine/src/Components/AudioSource.cpp
engine/src/Components/AudioListener.cpp
```

### Tipos compartidos (`AudioTypes.h`)
```cpp
using SoundId = uint32_t;                    // id opaco de fuente/reproductor
inline constexpr SoundId kInvalidSoundId = 0;
using ClipId = uint32_t;                     // handle de clip (backend)

enum class SoundState : uint8_t {
    Stopped = 0,   // no reproduce
    Playing = 1,
    Paused = 2,
    Loading = 3,   // clip decodificando
};
```

### IAudioBackend (`AudioBackend.h`) — método por método propios del motor
```cpp
class IAudioBackend {
    virtual bool Init() = 0;                       // abre el device (no en web)
    virtual void Shutdown() = 0;
    virtual void Update(float dt) = 0;             // faders + update3dAudio
    virtual bool WakeUp() = 0;                     // (web) arranca device en 1er gesto

    virtual ClipId LoadClip(const std::string& path) = 0;
    virtual void UnloadClip(ClipId clip) = 0;
    virtual float GetClipDuration(ClipId clip) const = 0;

    virtual SoundId CreateSource() = 0;
    virtual void DestroySource(SoundId source) = 0;
    virtual void Play(SoundId source, ClipId clip) = 0;
    virtual void Stop(SoundId source) = 0;
    virtual void Pause(SoundId source) = 0;
    virtual void Resume(SoundId source) = 0;
    virtual void Seek(SoundId source, double seconds) = 0;
    virtual void SetLooping(SoundId source, bool loop) = 0;
    virtual void SetVolume(SoundId source, float volume) = 0;
    virtual void SetPitch(SoundId source, float pitch) = 0;
    virtual void FadeVolume(SoundId source, float targetVolume, float seconds) = 0;

    virtual void SetSource3D(SoundId source, const Vector3& position) = 0;
    virtual void SetListener3D(const Vector3& position, const Vector3& forward, const Vector3& up) = 0;
    virtual bool Supports3D() const = 0;

    virtual SoundState GetState(SoundId source) const = 0;
    virtual double GetTime(SoundId source) const = 0;

    virtual void SetMasterVolume(float volume) = 0;
    virtual float GetMasterVolume() const = 0;
};
```

### SoLoudBackend
- Mapea a `SoLoud::Soloud` + `SoLoud::Wav` (decodifica completo en memoria; los
  assets del demo son chicos; `WavStream` queda para música larga futura).
- Un `SoundId` = una voz: `Play` = `stop()` previo + `play()`/`play3d()` + `setLooping`,
  `Pause/Resume` = `setPause`, `Stop` = `stop`, `Seek` = `seek`, `GetTime` = `getStreamTime`.
- 3D: `play3d()` cuando el source es espacial; `set3dSourcePosition` para moverlo;
  `Update()` llama `soloud.update()` (faders) + `soloud.update3dAudio()`.
- Listener: `set3dListenerParameters(pos, at=pos+fwd, up)`.
- **Web**: `Init()` NO llama `soloud.init()` (el AudioContext nace suspendido);
  `WakeUp()` lo inicia en el primer gesto. Desktop: `Init()` inicia al toque, `WakeUp()` no-op.
- Master volume: `setGlobalVolume`. Fades: `setFadeVolume`.

### AudioEngine (singleton)
```cpp
class AudioEngine {
    static AudioEngine& GetInstance();
    void Init();                 // crea el IAudioBackend por plataforma
    void Shutdown();
    void Update(float dt);       // lo llama el app/demo (o Scene::OnUpdate)
    void WakeUp();               // (web) primer gesto del usuario
    bool IsInitialized() const;
    IAudioBackend* GetBackend();
    ClipId GetOrLoadClip(const std::string& path);   // caché: decodifica 1 vez
    std::shared_ptr<AudioClip> GetClipAsset(const std::string& path);
    void UnloadAllClips();
};
```

### AudioClip (asset)
`AudioClip::Load(path)` → `shared_ptr<AudioClip>` (con caché en AudioEngine).
`GetPath()`, `GetDuration()` (segundos), `GetClipId()`.

### AudioSource (componente, estilo Unity)
```cpp
SetClip(shared_ptr<AudioClip>) / SetClipPath(path) / GetClip()
Play(); Stop(); Pause(); Resume(); Seek(double seconds)
SetLooping(bool) / IsLooping();  SetVolume(float volume) / GetVolume()
SetPitch(float) / GetPitch();    SetPlayOnAwake(bool) / GetPlayOnAwake()
SetSpatial3D(bool) / IsSpatial3D()
SetMinDistance(float) / SetMaxDistance(float)      // falloff 3D
IsPlaying(); GetState() → SoundState;  GetTime() / GetDuration()
// OnAwake → CreateSource (+Play si playOnAwake en OnStart)
// OnUpdate → sync pos 3D desde el Transform si Spatial3D
// OnDestroy → DestroySource
```

### AudioListener (componente)
`OnUpdate` → `SetListener3D(GetWorldPosition(), GetForward(), GetUp())`.

### SoundPlayer (API estática rápida, patrón XConsole/Keyboard)
Overloads estilo C#: cortas para lo común, largas solo cuando se necesita. `SoundId`
convertible desde int (`Play(1, "x")`). Auto-ids de `Play(path)` salen de un contador
monotónico con base alta (`kAutoIdBase`) → nunca chocan con ids de usuario chicos.
**`volume` como nombre unificado en toda la API** (decisión del usuario). Música =
**un canal activo a la vez**; `StopMusic()`/`PauseMusic()` sin id actúan sobre la actual.

```cpp
// --- Reproducir SFX ---
static SoundId Play(std::string_view path);                            // 2D one-shot → SoundId auto
static SoundId Play(std::string_view path, const Vector3& pos);        // 3D
static SoundId Play(SoundId id, std::string_view path);
static SoundId Play(SoundId id, std::string_view path, const Vector3& pos);
static SoundId Play(SoundId id, bool loop, std::string_view path);
static SoundId Play(SoundId id, bool loop, std::string_view path, const Vector3& pos);
static SoundId Play(SoundId id, bool loop, float volume, std::string_view path);
static SoundId Play(SoundId id, bool loop, float volume, std::string_view path, const Vector3& pos);
static SoundId PlayOneShot(std::string_view path);                     // alias de Play(path)

// --- Control por id ---
static void Stop(SoundId id);  static void Pause(SoundId id);  static void Resume(SoundId id);
static void Stop();            static void Pause();            static void Resume();  // = último Play()
static void Seek(SoundId id, double seconds);
static void SetLoop(SoundId id, bool loop);  static bool GetLoop(SoundId id);
static void SetVolume(SoundId id, float volume);  static float GetVolume(SoundId id);
static void SetPitch(SoundId id, float pitch);   static float GetPitch(SoundId id);
static void SetPosition(SoundId id, const Vector3& pos);   // mueve un sonido 3D
static void FadeOut(SoundId id, float seconds);  static void FadeTo(SoundId id, float volume, float seconds);

// --- Estado ---
static SoundState GetState(SoundId id);  static bool IsPlaying(SoundId id);
static double GetTime(SoundId id);       static float GetDuration(SoundId id);
static SoundId GetPlayingSound();        // último quick-play activo (o kInvalidSoundId)

// --- Música (DEFAULT LOOP = true) ---
static SoundId PlayMusic(SoundId id, std::string_view path);        // loop = true
static SoundId PlayMusic(SoundId id, bool loop, std::string_view path);
static void StopMusic(SoundId id);   static void StopMusic();
static void PauseMusic(SoundId id);  static void PauseMusic();
static void ResumeMusic(SoundId id); static void ResumeMusic();
static void SetMusicLoop(bool loop); static bool GetMusicLoop();
static void SetMusicVolume(float volume); static float GetMusicVolume();
static SoundState GetMusicState();   static SoundId GetMusic();

// --- Global ---
static void SetMasterVolume(float volume);  static float GetMasterVolume();
static void StopAll();       // efectos + música
static void PauseAll();      static void ResumeAll();
static void FadeAll(float seconds);
```

### Semántica interna de SoundPlayer
- Registro `SoundId → {backend source, isMusic}` (static, patrón function-local
  statics como `XConsole`).
- El quick-play sin id **reusa un único slot** (`g_LastQuick`): `Play(path)` reinicia
  la misma fuente → memoria acotada y `Stop()`/`Pause()` sin argumentos son naturales.
- `Play(id, ...)` con id nuevo → `CreateSource`; con id existente → reinicia (stop previo).
- Si un id es de música y se usa para efecto (o viceversa), se hace switch limpio.
- `GetPlayingSound()` = `g_LastQuick` si su estado backend != `Stopped`; si no, `kInvalidSoundId`.

### Ampliaciones profesionales (incluidas)
- **Fades nativos de SoLoud** (`FadeOut`/`FadeTo`/`FadeAll`).
- **Seek/GetTime/GetDuration/GetState** por id y por componente.
- **Caché de clips por path** (mismo WAV/OGG reproducido N veces decodifica 1 vez).
- **WakeUp** → resuelve la autoplay policy de web (primer click del demo).
- **Master volume + PauseAll/ResumeAll/StopAll/FadeAll** globales.
- 3D completo: `AudioSource` espacial + `AudioListener` + falloff min/maxDistance;
  sin `pos` el sonido es **2D**.
- Futuro (anotado, no en M4): `AudioBus`/grupos mixer, DSP filters, variación aleatoria
  de pitch, playlist, `PlayDelayed`.

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
- Agregar a `target_sources` del engine: `src/Audio/AudioEngine.cpp`,
  `src/Audio/AudioClip.cpp`, `src/Audio/SoLoudBackend.cpp`,
  `src/Audio/SoundPlayer.cpp`,
  `src/Components/AudioSource.cpp`, `src/Components/AudioListener.cpp`.

### Web (`examples/WebEngineDemo/CMakeLists.txt` + `engine/CMakeLists.web.txt`)
- Agregar fetch de SoLoud (mismo patrón que Jolt) + crear el target `soloud` con
  backend `miniaudio` + `-std=gnu++20`.
- Agregar las fuentes de audio a `LeirEngineCore` + link `PRIVATE soloud`.
- `--preload-file` de `assets@/assets` ya cubre `assets/audio/`.

## Gotchas (estado tras la implementación 2026-08-17)

1. **Autoplay policy del browser** (✅ resuelto): el `AudioContext` nace suspendido
   y el browser exige un **gesto del usuario** para `resume()`. Resuelto con
   **init lazy**: `AudioEngine::WakeUp()` inicia el device en el primer gesto; el
   WebEngineDemo llama `WakeUp()` + `PlayMusic(...)`/`Play(...)` en el primer
   `OnPointerDown` (click-to-enable-audio). miniaudio además registra su propio
   unlock en los eventos `click`/`touchstart`/`touchend` (creado con `suspend()`,
   `ma_device_start` → `resume()`).
2. **miniaudio en Emscripten** (✅ resuelto): NO compila con `-std=c*` estricto ni
   `-ansi` (miniaudio.h:276,1466) → el TU del backend miniaudio se compila con
   `-std=gnu++20`. Verificado contra emsdk 6.0.6 (build web verde).
3. **Pin de SoLoud** (✅ hecho): `GIT_TAG master` → pin a commit **`e82fd32c`**.
4. **Streaming en web**: todo el asset vive en memoria (preload) — `WavStream`
   funciona sobre el archivo preloadado; no hay streaming real por red en el demo.
5. **Desktop NO compila SoLoud hoy** (✅ resuelto): sin CMakeLists en el repo →
   target propio `soloud` STATIC creado desde `${soloud_SOURCE_DIR}`.
6. **Exports del módulo JS en el build web (3 fixes encadenados, ✅ resuelto)** —
   el backend **webaudio** de miniaudio ejecuta EM_ASM JS que accede a
   `Module.*`; en Emscripten 4.x solo los símbolos listados en
   `EXPORTED_FUNCTIONS` reciben copia `Module[name]`, y `EXPORTED_RUNTIME_METHODS`
   por defecto es `[]`:
   - **`Module._malloc`/`_free`** → `Uncaught TypeError: Module._malloc is not a
     function`. miniaudio reserva el buffer intermedio con `Module._malloc`/`_free`
     (miniaudio.h L32306-32327). Fix: `-sEXPORTED_FUNCTIONS=_malloc,_free,_main`.
   - **`Module.HEAPF32`** → `Aborted('HEAPF32' was not exported. add it to
     EXPORTED_RUNTIME_METHODS ...)`. El mismo EM_ASM construye la vista del buffer
     como `new Float32Array(Module.HEAPF32.buffer, ...)`. Fix:
     `-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF64`.
   - **`ccall`** → `Uncaught TypeError: ccall is not a function` lanzado en
     `onaudioprocess` (ScriptProcessorNode): el callback invoca el C
     `ma_device_process_pcm_frames_playback__webaudio` vía `ccall(...)` (EM_ASM_CONST
     16962705). Fix: agregar `ccall` a `EXPORTED_RUNTIME_METHODS`.
   - **Reglas aprendidas**: (a) si se especifica `EXPORTED_FUNCTIONS` hay que
     incluir `_main` (si no → `EXPECT_MAIN=0` y main no corre); (b) los exports
     auto-generados (KEEPALIVE `ma_device_process_pcm_frames_*`, `emwgpuOn*`,
     dynCall, stack) se conservan aparte — no se rompen al fijar la lista.

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

## Decisiones del usuario (cerradas 2026-08-17)
1. **Audio 3D**: ✅ completo — `AudioSource` espacial + `AudioListener` + falloff
   min/maxDistance; sin `pos` el sonido es 2D.
2. **Demo**: ✅ en `WebEngineDemo` (música loop + SFX por click).
3. **Assets**: ✅ genero WAV+OGG CC0 con ffmpeg (disponible, v8.1.2).
4. **Naming**: ✅ `volume` unificado en toda la API (no `level`).
5. **GetPlayingSound()**: ✅ último quick-play activo (o `kInvalidSoundId`).
6. **Overloads 3D**: ✅ 8 overloads de `Play` con `Vector3`.

## Fases sugeridas (todas completas 2026-08-17)
1. ✅ Investigación hecha. Target `soloud` propio (wasapi/null/miniaudio) +
   validado contra emsdk 6.0.6 (`-std=gnu++20`).
2. ✅ `IAudioBackend` + `SoLoudBackend` + `AudioEngine` + `AudioClip` (desktop).
3. ✅ `AudioSource`/`AudioListener` + sync 3D.
4. ✅ `SoundPlayer` (API estática).
5. ✅ Assets + integración builds (desktop + web).
6. ✅ Demo WebEngineDemo (click→WakeUp→música + SFX) + verificación Firefox
   (usuario: sonidos OK) y desktop (build verde).
7. ✅ Docs + CI + commit (sin `[skip ci]`).
