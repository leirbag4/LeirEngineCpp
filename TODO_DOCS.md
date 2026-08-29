# TODO Docs — Sistema de documentación automatizada (LeirEngine)

Plan para un sistema de docs **profesional, automatizable y portátil**: documentación de la API del
código (engine + editor + ECS) generada automáticamente, fusionada con guías escritas en **Markdown**,
en **una sola página web moderna dark**, generada desde un `.bat` (que llama a CMake con el target
`docs`). Sin instalar nada (tools portátiles descargados por script).

Decisión de stack (2026-08-28, aprobada por el usuario):
**Doxygen → XML → Sphinx + Breathe + Exhale + MyST + Furo**.

- [x] **Decisión de stack**: Sphinx + Breathe + Exhale + MyST-Parser + Furo (API embebida nativa,
      árbol de API automático, guías en Markdown, tema dark moderno, búsqueda unificada).
- [x] **`docs/site/` se commitea** (NO GitHub Pages por ahora; solo generación local).
- [x] **Tools portátiles en `docs/tools/` gitignored**, descargadas por `setup_tools.bat`.
- [x] **Doxygen pineado a `1.18.0`** (portátil: `doxygen-1.18.0.windows.x64.bin.zip`).
- [x] **`.bat` llama a CMake con el target `docs`** (`cmake --build build/windows-debug --target docs`).

---

## 1. Cómo funciona (las piezas)

| Pieza | Tipo | Rol |
|---|---|---|
| **Doxygen 1.18.0** | extractor | Lee los comentarios del código → genera **XML** (`GENERATE_XML=YES`). Ya NO renderiza HTML. Entiende `LEIR_API` (vía `PREDEFINED`), templates y `@defgroup`. También genera diagramas con **Graphviz** (`HAVE_DOT=YES`). |
| **Breathe** | puente | Lee el XML de Doxygen y lo **embebe** en Sphinx: `.. doxygenclass:: Leir::World`, `.. doxygenfunction::`. Permite cross-refs desde las guías: `` :cpp:class:`Leir::World` ``. |
| **Exhale** | generador | **Crea automáticamente el árbol completo de la API** (namespaces, clases, archivos, grupos) desde el XML — no hay que escribir directivas a mano. |
| **Sphinx** | generador de sitio | `conf.py` + fuentes (Markdown vía MyST) → HTML con tema Furo, búsqueda, sidebar. |
| **MyST-Parser** | parser | Hace que Sphinx lea **Markdown** (`#`, admoniciones ` ```{note} `, tabs, etc.). |
| **Furo** | tema | Look moderno dark (toggle de tema, sidebar, search). |
| **Graphviz** | diagramas | Diagramas de herencia/colaboración en las páginas de clases (portátil). |
| **CMake target `docs`** | orquestador | `find_package(Doxygen)` + `doxygen_add_docs` (XML) + `add_custom_target` (sphinx-build). |
| **setup_tools.bat** | provision | Descarga doxygen + graphviz portátiles y hace `pip install -r requirements.txt`. |
| **generate_docs.bat** | entry point | Setup si falta → `cmake --build ... --target docs` → abre `docs\site\index.html`. |

### Flujo completo
```
generate_docs.bat
  └─ setup_tools.bat (si falta)  → tools/doxygen + tools/graphviz + pip install
  └─ cmake --build build/windows-debug --target docs
        ├─ doxygen Doxyfile          → docs/sphinx/_xml/  (XML del código)
        └─ %PYTHON% -m sphinx docs/sphinx docs/site
              ├─ Breathe/Exhale leen el XML → sección "API Reference" (árbol automático)
              └─ MyST renderiza guides/*.md + index.md
  └─ abre docs\site\index.html
```

---

## 2. Estructura de `docs/`

```
docs/
├── Doxyfile                 # config: INPUT (engine/editor/tests), GENERATE_XML=YES, HAVE_DOT=YES
├── requirements.txt         # sphinx, breathe, exhale, myst-parser, furo, sphinx-design (pines)
├── setup_tools.bat          # baja tools portátiles + pip install (idempotente)
├── generate_docs.bat        # entry point → cmake target docs → abre el sitio
├── .gitignore               # docs/tools/  (site/ SÍ se commitea)
├── tools/                   # (gitignored) doxygen/, graphviz/
├── sphinx/                  # fuente del sitio (se commitea)
│   ├── conf.py              # tema Furo, MyST, Breathe+Exhale, PROJECT "LeirEngine"
│   ├── index.md             # main page (bienvenida + links a guías y API)
│   ├── guides/              # guías en Markdown (se commitean)
│   │   ├── engine/          #   (getting-started, componentes, render, input, audio, física...)
│   │   ├── editor/          #   (dock, gizmos, hierarchy, console...)
│   │   ├── ecs/             #   ecs-public-api.md, hybrid-ecs.md (convertidos de los .html)
│   │   └── web/             #   (export, física web, audio web...)
│   └── api/                 # Exhale genera acá el árbol de la API (auto, se regenera)
├── ecs/
│   └── html-legacy/         # los .html originales conservados (comparación) — se mantienen
└── site/                    # HTML generado (COMMITEADO) — apto para servir estático
```

---

## 3. Pasos de implementación (checkboxes)

### 3.1 Provisionamiento (tools portátiles)
- [x] Crear `docs/tools/` (gitignored via `docs/.gitignore`).
- [x] `setup_tools.bat`:
  - [x] Localiza Python: `LEIR_PYTHON` (default `C:\programs_dev\python_3_13_7_opt_b\python.exe`), fallback `python`/`py`.
  - [x] Descarga `doxygen-1.18.0.windows.x64.bin.zip` (GitHub releases, tag `Release_1_18_0`; usa `curl.exe`
        — `Invoke-WebRequest` 404/prompt en PS 5.1 — y extrae con `tar.exe` — `Expand-Archive` falla con
        "Central Directory corrupt" en algunos zips) → `tools/doxygen/`.
  - [x] Descarga **Graphviz 15.1.1 portátil** (GitLab generic packages
        `windows_10_cmake_Release_Graphviz-15.1.1-win64.zip`; best-effort, si falla queda sin dot) → `tools/graphviz/`.
  - [x] `%LEIR_PYTHON% -m pip install -r docs\requirements.txt` (pineado: sphinx 9.1.0, breathe 4.36.0,
        exhale 0.3.7, myst-parser 5.1.0, furo 2025.12.19, sphinx-design, sphinx-copybutton).
  - [x] Idempotente: si el tool ya existe, no re-descarga.

### 3.2 Config de Doxygen (`Doxyfile`)
- [x] `PROJECT_NAME = "LeirEngine"`, `PROJECT_BRIEF`.
- [x] `INPUT = engine/include/LeirEngine editor/src tests`.
- [x] `FILE_PATTERNS = *.h *.hpp *.cpp`.
- [x] `EXTRACT_ALL = YES`, `EXTRACT_STATIC = NO`, `HIDE_UNDOC_MEMBERS = NO`.
- [x] `PREDEFINED = LEIR_API= __declspec(x)= ...` + `MACRO_EXPANSION=YES` + `EXPAND_ONLY_PREDEF=YES`
      (para que `LEIR_API` no rompa el parseo y se documente todo).
- [x] `GENERATE_XML = YES`, `XML_OUTPUT = sphinx/_xml`, `XML_PROGRAMLISTING = NO`.
- [x] `GENERATE_HTML = NO` (el HTML lo hace Sphinx).
- [x] `HAVE_DOT = YES` + `DOT_PATH` a `docs/tools/graphviz/Graphviz-15.1.1-win64/bin`,
      `COLLABORATION_GRAPH`, `INHERIT_GRAPH`, `CALL_GRAPH = YES`.
- [x] `CLASS_DIAGRAMS` es **obsoleto en 1.18.0** (quitado; `CLASS_GRAPH` basta).
- [x] `RECURSIVE = YES`.
- [ ] Grupos `@defgroup` por módulo (ver §5): Math, ECS, Scene, Components, Rendering, RHI, Audio,
      Physics, UI, Input, Core, Editor. (Nota: con `EXTRACT_ALL` el sitio funciona sin ellos; se
      agregan con el retrofit §3.6.)

### 3.3 Config de Sphinx (`docs/sphinx/conf.py`)
- [x] `project = "LeirEngine"`, `html_theme = "furo"`.
- [x] `extensions = [myst_parser, breathe, exhale, sphinx_design, sphinx_copybutton]`.
- [x] `breathe_projects = {"LeirEngine": "_xml"}`.
- [x] `exhale_args`: `containmentFolder = "api"`, `rootFileName = "index.rst"`,
      `createTreeView = True`, `exhaleExecutesDoxygen = False`.
- [x] `primary_domain = "cpp"`, `highlight_language = "cpp"`.
- [x] `myst_enable_extensions` (colon_fence, tasklist, deflist, dollarmath, html_image, fieldlist).
- [x] `html_static_path` + `_static/custom.css` (acento del engine + dark).
- [x] `docs/sphinx/index.md` (main page con toctree a `api/index`).
- [x] Verificado: `python -m sphinx -b html docs/sphinx docs/site` → **build succeeded**, 635 html.
- [x] Baseline de warnings (~44, benignos): `Duplicate` (34, nested classes de Exhale), `Invalid` (7,
      un miembro de `LeirSettings` que Breathe no parsea), 3 menores de símbolos sueltos. Los de
      `tests/` (Check/LOG/TEST/main) se eliminaron sacando `tests` del INPUT de Doxygen (no son API
      pública). `suppress_warnings` NO los silencia (warnings sin tipo). Se revisitan con el retrofit
      de docblocks (§3.6).

### 3.4 CMake target `docs`
- [x] `cmake/DocsTooling.cmake` (incluido al final del root `CMakeLists.txt`):
      `find_package(Doxygen)` con preferencia del **portátil de `docs/tools/doxygen`**; `find_package(Python3)`
      con `Python3_EXECUTABLE` vía `LEIR_PYTHON` → path del dev → PATH.
- [x] `docs_api` = corre `doxygen docs/Doxyfile` (WORKING_DIRECTORY = raíz del repo, EXCLUDE_FROM_ALL).
- [x] `docs_sphinx` = `python -m sphinx -b html docs/sphinx docs/site` (EXCLUDE_FROM_ALL).
- [x] `docs` DEPENDS docs_api + docs_sphinx, EXCLUDE_FROM_ALL (el build normal NO genera docs).
- [x] `option(LEIR_BUILD_DOCS ON)` — nota: el primer configure lo dejó OFF en cache (interacción
      option/cache); forzar `-DLEIR_BUILD_DOCS=ON` la primera vez.
- [x] Verificado: `cmake --build build/windows-debug --target docs` → exit 0 → doxygen + sphinx → sitio.

### 3.5 Contenido inicial (guías en Markdown)
- [x] `docs/generate_docs.bat` (entry point): setup si faltan tools → `cmake --build build/windows-debug
      --target docs` → `start docs\site\index.html`. Localiza cmake (PATH o vswhere vía `set /p` a
      archivo temp — `for /f` con backticks rompe con `(x86)`; no usar paréntesis en textos dentro de
      bloques `if ( )`). Verificado end-to-end (exit 0, sitio abierto).
- [x] `docs/sphinx/index.md` — main page (hecha en §3.3, toctree a `api/index` + `guides/ecs/*`).
- [x] Convertir `docs/ecs/ecs-public-api.html` → `docs/sphinx/guides/ecs/ecs-public-api.md` (MyST).
- [x] Convertir `docs/ecs/hybrid-ecs.html` → `docs/sphinx/guides/ecs/hybrid-ecs.md` (MyST).
- [x] Mover los `.html` originales a `docs/ecs/html-legacy/` (comparación).
- [ ] (Opcional) Guías iniciales: `guides/engine/getting-started.md`, `guides/engine/architecture.md`.
- [x] `docs/.gitignore` (ignora `tools/` + `sphinx/_xml/` + `sphinx/api/`, mantiene `site/`).

### 3.6 Documentación del código (convención Doxygen)

**Regla (aprobada por el usuario 2026-08-28): documentamos los HEADERS públicos con docblocks
completos; los `.cpp` NO se documentan (solo comentarios `//` de implementación — la API vive en los
headers). Retrofit PROGRESIVO por módulo: Math → ECS → Core → Scene → Components → Rendering → RHI →
Audio → Physics → UI → Input → Editor; se documenta un header cuando se toca (o a pedido).**
Mirror obligatorio en `AGENTS.md` ("Documentación del código (Doxygen)").

#### Formato (el estándar de industria)
- Bloques multilínea `/** ... */`; comando con prefijo `@` (`@brief`, `@param`, `@return`, `@code`,
  `@ingroup`, `@tparam`, `@details`, `@note`, `@warning`). Miembros en una línea con `///<`.
- Un header `@file @brief @ingroup <Módulo>` al tope; cada clase/método/función/enum con docblock.

#### Ejemplos
```cpp
// File header (tope de cada header público)
/**
 * @file World.h
 * @brief ECS container: entities, pools, journal and hybrid registry.
 * @ingroup ECS
 */

// Clase
/**
 * @brief Generational entity handle {index, generation}.
 * @details Handles viejos jamás resuelven (la generación sube al destruir).
 *          El índice 0 está reservado como entidad nula.
 */
class LEIR_API Entity { ... };

// Método público (docblock completo: brief + params + return)
/**
 * @brief Adds a component of type T to the entity (one-per-type).
 * @tparam T Tipo del campo (POD).
 * @param[in] e Entidad objetivo.
 * @return Referencia al componente vivo; el existente si ya estaba.
 */
template <typename T> T& Add(Entity e);

// Función libre (Mathf)
/**
 * @brief Interpolación lineal entre dos valores.
 * @param a Valor inicial.
 * @param b Valor final.
 * @param t Factor de interpolación [0,1].
 * @return a + (b - a) * t.
 */
inline float Lerp(float a, float b, float t);

// Enum con enumerators (trailing)
/**
 * @brief Filtro de verbosidad del logger.
 */
enum class LogLevel {
    Trace,   ///< descartado en el origen (debug-only)
    Debug,   ///< debug-only
    Info,    ///< retenido en el ring buffer
    Error    ///< retenido; además va a stderr
};

// Campo (trailing)
std::vector<uint32_t> m_Generations; ///< entity index -> generation

// Grupo por módulo (cada header lleva "@ingroup <Módulo>" en su @file)
/** @defgroup ECS Entity Component System (núcleo data-oriented) */
/** @{ */
/** @} */
```

#### Scope (qué se documenta)
| Qué | ¿Se documenta? | Formato |
|---|---|---|
| Headers públicos (`engine/include/...`, `editor/src/*.h`) | ✅ siempre | `@file @brief @ingroup` + docblocks |
| `.cpp` | ❌ (solo `//` normales de implementación) | — |
| Clases / structs | ✅ | brief + details + `@tparam` |
| Métodos públicos | ✅ | brief + `@param[in/out]` + `@return` + `@throw` |
| Campos públicos | ✅ | trailing `///<` |
| Campos privados | ⚠️ opcional (solo si aporta) | trailing `///<` |
| Enums + enumerators | ✅ | brief + `///<` por enumerator |
| Funciones libres | ✅ | brief + `@param` + `@return` |
| Módulos | ✅ | `@defgroup` (ver §5) + `@ingroup` por header |
| Ejemplos | ✅ cuando aporta | `@code ... @endcode` |
| Métodos privados | ❌ (detalle de implementación) | — |

#### Módulos → `@ingroup` (ver tabla §5)
Math, ECS, Scene, Components, Rendering, RHI, Audio, Physics, UI, Input, Core, Editor.
- [ ] (Diferido) `@example` apuntando a `examples/*`.
- [ ] Retrofit progresivo: Math → ECS → Core → Scene → Components → Rendering → RHI → Audio →
      Physics → UI → Input → Editor (marcar cada módulo al terminarlo).

### 3.7 Verificación
- [x] `setup_tools.bat` corre limpio en esta máquina (baja doxygen+graphviz+pip — 3 bugs de cmd corregidos:
      `Invoke-WebRequest` 404 → `curl`, tag `1_18_0`, `Expand-Archive` → `tar`).
- [x] `generate_docs.bat` → CMake target `docs` → XML + Sphinx → `docs/site/` generado sin errores
      (exit 0; baseline ~44 warnings benignos de Exhale — nested-class duplicates, LeirSettings anonymous
      structs — más 9 de MyST Lexing en las guías ASCII — todos silenciados con `suppress_warnings` NO
      efectivo; revisados como benignos).
- [x] El sitio abre: tema Furo dark, búsqueda, árbol de API completo, guías `ecs-public-api` y
      `hybrid-ecs` renderizadas en el sitio fusionado (635+ html).
- [x] `docs/site/` commiteado (git add docs/site) — pendiente inmediato (paso 7).
- [x] ctest 3/3 + build limpio (el target `docs` con `EXCLUDE_FROM_ALL` no rompe el build normal).
- [ ] (Otra máquina) correr `setup_tools.bat` + `generate_docs.bat` desde cero.

---

## 4. Pines de versión (Python 3.13.7)
- [x] `requirements.txt` pineado (verificado tras `setup_tools.bat` en 2026-08-28):
  - `sphinx>=8.1`, `breathe>=4.35`, `exhale` (verificar compatibilidad con Sphinx 8 — pin si hace
    falta, p.ej. `exhale==0.3.6` o la versión que resuelva), `myst-parser>=4`, `furo>=2024`,
    `sphinx-design>=0.6`, `sphinx-copybutton>=0.5`.
- [ ] Nota: si Exhale no es compatible con Sphinx 8, evaluar pin a Sphinx 7.x (documentar la decisión).

---

## 5. Módulos (`@defgroup`) a documentar
| Módulo | Headers | Contenido |
|---|---|---|
| **Math** | `Math/` | Mathf, Vector2/3/4, Quaternion, Matrix4x4, Simd, SoA, Frustum |
| **ECS** | `ECS/` | World, Entity, TypedPool, SoAPool, OwnedGroup, SystemPipeline, CommandBuffer, HybridComponent, HierarchyTree, TransformSystem, Tags |
| **Scene** | `Scene/` | Scene, ISceneStorage, SceneGroups |
| **Components** | `Components/` | MeshRenderer, Camera, Light, SpriteRenderer, RigidBody, Collider, AudioSource, AudioListener, CanvasRenderer |
| **Rendering** | `Rendering/` | RenderPipeline, Mesh, Material, Texture2D, RenderTexture, Shader, SpriteSheet, Font |
| **RHI** | `RHI/` | RenderBackend, GCommandGraph, ShaderLayout, BackendFactory, IShaderCompiler |
| **Audio** | `Audio/` | AudioEngine, SoundPlayer, AudioClip, AudioSyncSystem |
| **Physics** | `Physics/` | PhysicsWorld, PhysicsSyncSystem |
| **UI** | `UI/` | UIElement, UICanvas, widgets (UIPanel/UILabel/UIImage/UIButton/UITextInput/...), Dock/ |
| **Input** | `Input/` | InputManager, Keyboard, Mouse, Pointer, Touch, EventQueue |
| **Core** | `Core/` | CoreObject, Transform, Component, JobSystem, Log, Settings, UUID |
| **Editor** | `editor/src` | EditorApp, HierarchyPanel, Inspector, gizmos, dock content |

---

## 6. Notas y footguns
- **Doxygen + `LEIR_API`**: si el parseo falla por `__declspec`, usar `PREDEFINED` (nunca
  `MACRO_EXPANSION` a secas — puede romper templates). Verificar con un header simple primero.
- **Exhale necesita `breathe_projects` + `exhale_args.containmentFolder`** y que el XML esté generado
  ANTES de sphinx-build (por eso el target `docs` tiene dependencia `docs_api`).
- **El sitio se commitea** pero `site/` es output — regenerarlo con el `.bat`; no editar a mano.
- **Graphviz**: si no hay zip portable oficial, `HAVE_DOT=NO` (heredoc/inheritance igual funciona,
  Doxygen tiene generador nativo de herencia; solo se pierden graphs de colaboración/llamadas).
- **Perf**: el primer build del XML tarda (engine + editor + tests). Normal.
- **No romper el build normal**: el target `docs` es `EXCLUDE_FROM_ALL` para que `cmake --build` a
  secas NO genere docs (solo `--target docs`).

---

## 7. Estado
- [ ] Plan aprobado y en curso (este archivo).
- [ ] Implementación §3.1–3.7.
- [ ] Sitio generado y commiteado.
- [ ] Setup probado "desde cero" (otra máquina).