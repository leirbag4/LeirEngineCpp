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
- [ ] Crear `docs/tools/` (gitignored).
- [ ] `setup_tools.bat`:
  - [ ] Localiza Python: `LEIR_PYTHON` (default `C:\programs_dev\python_3_13_7_opt_b\python.exe`), fallback `python`/`py`.
  - [ ] Descarga `doxygen-1.18.0.windows.x64.bin.zip` (GitHub releases) → extrae a `tools/doxygen/`.
  - [ ] Descarga **Graphviz portátil** (zip de Windows de los release assets; si no hay zip oficial
        portable, se deja `HAVE_DOT=NO` y Doxygen genera herencia nativa sin colaboración) → `tools/graphviz/`.
  - [ ] `%LEIR_PYTHON% -m pip install -r docs\requirements.txt` (idempotente).
  - [ ] Idempotencia: si el tool ya está, no re-descarga (marca `tools/.ready`).

### 3.2 Config de Doxygen (`Doxyfile`)
- [ ] `PROJECT_NAME = "LeirEngine"`, `PROJECT_BRIEF`.
- [ ] `INPUT = engine/include/LeirEngine editor/src tests` (+ docs/sphinx/guides si se quiere como páginas MD).
- [ ] `FILE_PATTERNS = *.h *.hpp *.cpp`.
- [ ] `EXTRACT_ALL = YES`, `EXTRACT_STATIC = NO`, `HIDE_UNDOC_MEMBERS = NO`.
- [ ] `PREDEFINED = LEIR_API= __declspec(dllexport)= __declspec(dllimport)=` + `MACRO_EXPANSION=YES`
      (para que `LEIR_API` no rompa el parseo y se documente todo).
- [ ] `GENERATE_XML = YES`, `XML_OUTPUT = sphinx/_xml`, `XML_PROGRAMLISTING = NO`.
- [ ] `GENERATE_HTML = NO` (el HTML lo hace Sphinx).
- [ ] `HAVE_DOT = YES` + `DOT_PATH` a `tools/graphviz` (si disponible), `CLASS_DIAGRAMS = YES`,
      `COLLABORATION_GRAPH = YES`, `INHERIT_GRAPH = YES`, `CALL_GRAPH = YES`.
- [ ] `GROUP_GRAPHS = YES`, `CALLER_GRAPH = NO` (perf).
- [ ] `RECURSIVE = YES`.
- [ ] Grupos `@defgroup` por módulo (ver §5): Math, ECS, Scene, Components, Rendering, RHI, Audio,
      Physics, UI, Input, Core, Editor.

### 3.3 Config de Sphinx (`docs/sphinx/conf.py`)
- [ ] `project = "LeirEngine"`, `html_theme = "furo"`.
- [ ] `extensions = [myst_parser, breathe, exhale, sphinx_design]`.
- [ ] `breathe_projects = {"LeirEngine": "_xml"}`.
- [ ] `exhale_args`: `containmentFolder = "api"`, `rootFileName = "index.rst"`,
      `createTreeView = True`, `afterTitleDescription` breve.
- [ ] `primary_domain = "cpp"`, `highlight_language = "cpp"`.
- [ ] `myst_enable_extensions` (colon_fence, tasklist, deflist, html_image, etc.).
- [ ] `html_static_path` + custom CSS (opcional, para pulir el dark).

### 3.4 CMake target `docs`
- [ ] En `CMakeLists.txt` raíz (o `cmake/DocsTooling.cmake`): `find_package(Doxygen REQUIRED)`
      + `find_package(Python3 REQUIRED)` con `Python3_EXECUTABLE` apuntando a `LEIR_PYTHON`.
- [ ] `doxygen_add_docs(docs_api ...)` → genera el XML (dependencia).
- [ ] `add_custom_target(docs_sphinx COMMAND ${Python3_EXECUTABLE} -m sphinx -b html ... )`.
- [ ] `add_custom_target(docs DEPENDS docs_api docs_sphinx)`.
- [ ] `generate_docs.bat`: `cmake --build build/windows-debug --target docs` + `start docs\site\index.html`.

### 3.5 Contenido inicial (guías en Markdown)
- [ ] `docs/sphinx/index.md` — main page.
- [ ] Convertir `docs/ecs/ecs-public-api.html` → `docs/sphinx/guides/ecs/ecs-public-api.md`.
- [ ] Convertir `docs/ecs/hybrid-ecs.html` → `docs/sphinx/guides/ecs/hybrid-ecs.md`.
- [ ] Mover los `.html` originales a `docs/ecs/html-legacy/`.
- [ ] (Opcional) Guías iniciales: `guides/engine/getting-started.md`, `guides/engine/architecture.md`.
- [ ] `docs/.gitignore` (ignora `tools/`, mantiene `site/`).

### 3.6 Documentación del código (convención)
- [ ] Los headers ya tienen comentarios `//`; convertirlos a Doxygen `///` o `/** */` en los públicos
      clave (o usar `EXTRACT_ALL` sin docblocks — documenta todo igual, mejor con docblocks).
- [ ] Agregar `@defgroup`/`@{ @}` por módulo en un header índice (`engine/include/LeirEngine/Modules.h`
      o en cada header).
- [ ] Docblocks con `@brief`, `@param`, `@return`, ejemplos con `@code`.
- [ ] (Diferido) `@example` apuntando a `examples/*`.

### 3.7 Verificación
- [ ] `setup_tools.bat` corre limpio en esta máquina (baja doxygen+graphviz+pip).
- [ ] `generate_docs.bat` → CMake target `docs` → XML + Sphinx → `docs/site/` generado sin errores
      (chequear warnings de Breathe/Exhale: símbolos sin resolver, etc.).
- [ ] El sitio abre: tema Furo dark, búsqueda, árbol de API completo (World, Entity, pools, groups,
      systems, transforms, math, UI, RHI...), guías renderizadas.
- [ ] `docs/site/` commiteado (git add docs/site).
- [ ] ctest 3/3 + build limpio (el target `docs` no debe romper el build normal).
- [ ] (Otra máquina) correr `setup_tools.bat` + `generate_docs.bat` desde cero.

---

## 4. Pines de versión (Python 3.13.7)
- [ ] `requirements.txt` con versiones verificadas para Python 3.13 / Sphinx 8:
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