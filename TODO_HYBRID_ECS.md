# TODO Hybrid ECS — escena amigable + ECS data-oriented invisible

Plan maestro del rediseño de arquitectura: **el editor y el usuario final siguen usando la API OOP
amigable (`CoreObject`, `AddChild`, `GetChildren`, `SetParent`, `AddComponent<T>`)** y **por detrás todo
el almacenamiento y los sistemas calientes corren data-oriented** (ECS propio, SIMD, multithreading).

Estado: **en planificación** (2026-08-28). La motivación NO es "el editor es nuestro caso de uso dominante":
queremos el motor **más rápido y eficiente en todos los frentes** — editor, runtime, export web, mobile a
futuro — pensado a futuro, con acceso a datos cache-friendly, SIMD y paralelismo desde el día cero, sin
sacrificar la accesibilidad (estilo Unity/Unreal/Godot en la superficie).

Regla de oro: **la API pública es un contrato estable**. `obj.AddChild(child)`, `obj.GetChildren()`,
`obj.SetParent(parent, true)`, `obj.AddComponent<T>()`, `obj.GetComponent<T>()` se ven idénticos.
El ECS es un detalle de implementación. (Fase 3 agrega un tier "advanced ECS" opcional.)

---

## 1. Visión

Dos planos que coexisten unificados por el **mismo entity id**:

```
Scene (autoritativa)
├── Hierarchy tree (arrays: parent / firstChild / nextSibling)   ← los nodos SON entities
│     → AddChild / GetChildren / SetParent / GetParent = O(1) vía el tree
├── ECS world (propio, invisible)
│     ├── entity allocator (generational ids — reciclaje seguro)
│     ├── component pools (sparse set por tipo) + máscara de bits por entidad
│     ├── owned groups SoA (hot paths) alimentados por un journal de cambios estructurales
│     ├── query cache (With/Without) invalidado por cambios estructurales
│     └── systems pipeline (FixedUpdate / Update / Render) con scheduler paralelo (Fase 2)
└── Puente: CoreObject = handle delgado (entity id + World*) con la API amigable
```

- **El scene graph es el lugar de la ESTRUCTURA** (parent/children). No es un "componente" del ECS
  (a diferencia de flecs `ChildOf`): es un árbol propio compacto, O(1) para las operaciones del editor.
- **El ECS es el lugar de los DATOS y los SISTEMAS** (LocalTransform, WorldTransform, MeshRendererData,
  RigidBodyData, …). Los sistemas calientes iteran los pools/groups con acceso secuencial.

---

## 2. Investigación de industria (panorama 2025-2026)

| Motor | Authoring/editor | Runtime | ECS interno | Lección |
|---|---|---|---|---|
| **Unity** | GameObject + MonoBehaviour (OOP, scene graph) | GameObject + **DOTS** (opt-in) | Sí, separado: bake GO→Entity para simulación | "GameObjects para authoring/gameplay, DOTS para el core caliente". Nunca reemplazaron el scene graph |
| **Unreal** | Actor + ActorComponent (OOP, scene graph) | Actor + **Mass Entity** (archetype ECS) | Sí, apuntado: crowds/simulación masiva (Matrix Awakens) | ECS como subsistema especializado, sincronizado con Actor para el editor |
| **Godot** | Node tree (super amigable) | Node tree | No (ECS de comunidad vía GDExtension) | Scene tree OOP-friendly = el modelo de accesibilidad exitoso |
| **Bevy** | ECS puro (todo Entity/Component/System) | ECS | Sí, todo | ECS-first dificulta el editor; la accesibilidad es su reto |
| **flecs** | — | Archetype ECS con relationships/prefabs/reflection/JSON | Sí | Hierarchy (ChildOf) y prefabs (IsA) nativos; corre en browser (emscripten) |
| **entt** | — | Sparse-set ECS | Sí | Usado en Minecraft Bedrock; sin hierarchy/prefab/serialización nativos |

**Consenso**: scene graph para la jerarquía + ECS para la lógica que rinde, con una capa delgada de sync.
Nadie grande hizo ECS-first en el core authoring y siguió siendo amigable. Nosotros adoptamos el mismo
patrón pero **con un ECS 100% propio** (no copiamos flecs ni entt: diseñamos el almacenamiento híbrido
a nuestra medida, ver §3).

---

## 3. Análisis de almacenamiento: Archetypes vs Sparse Sets (decisión documentada)

Base: paper académico **"Run-time Performance Comparison of Sparse-set and Archetype Entity-Component
Systems"** — Cox, Williams, Vickers, Ward & Headleand, **CGVC 2025** (Univ. Staffordshire), prototipos
C++20, 5000 frames, escalando de 100 a 50 000 entidades. Resultados clave:

| Métrica | Archetype | Sparse Set |
|---|---|---|
| Frame update a 50k entidades | **~2× más rápido** (cache-coherent, contiguo) | ~2× más lento, **más varianza** (acceso indirecto) |
| Instanciación / add-remove componente | **6.6× más lento** (migración de tabla, memcpy) | **Más rápido** (O(1), swap-and-pop, sin migración) |
| Iteración multi-tipo | Secuencial pre-agrupada (SIMD-friendly) | Intersectar el pool menor + lookups aleatorios |
| Cambios estructurales frecuentes | Caros (re-memcpy del row) | Baratos |
| Componentes transitorios (eventos/tags) | Problemáticos (crean tablas/migraciones) | Triviales |
| Complejidad de implementación | Alta (gestor de tablas, grafo de aristas, query cache, chunk allocators) | Media, más simple |

**Carga de trabajo de LeirEngine** (editor + runtime + web + mobile):

| Carga | Patrón | Gana |
|---|---|---|
| Authoring (editor, undo/redo, prefabs) | Cambios estructurales constantes e interactivos + **reparent de subtrees** | Sparse set |
| Runtime (render, física, transform, audio) | Iterar miles/millones de entidades con filtros | Archetype (SoA) |

**DECISIÓN — almacenamiento HÍBRIDO PROPIO** (la síntesis, no copia de nadie):

1. **Base: pools sparse-set por tipo de componente.** Cambios estructurales O(1), reparent barato,
   simple, perfecto para el flujo interactivo del editor y para componentes transitorios.
2. **Hot paths: "owned groups" SoA** — arreglos contiguos por layout para los pocos loops que rinden
   (render list, transform propagation, cuerpos físicos, audio 3D). Actualizados **incrementalmente**
   por un **journal de cambios estructurales** (changelog dirty): un sistema marca "cambió la entidad E"
   y el grupo incorpora solo eso. Los loops calientes iteran arrays secuenciales (beneficio archetype)
   **sin** pagar migración por cada operación del editor.
3. **Queries** = iterar el grupo/pool más chico + máscara de bits por entidad; **query cache** invalidado
   por el journal.

Esto da: iteración archetype-like en los hot paths (SIMD-friendly, §5), cambios estructurales
sparse-set-like en el editor (O(1)), y componentes transitorios gratis.

---

## 4. Núcleo ECS custom

### 4.1 Entity
- **`uint32_t` índice + generación** (p.ej. `Entity { uint32_t index; uint32_t generation; }` — 64 bits en
  memoria o empaquetado a 32+generation aparte). Reciclaje seguro: al destruir una entidad se incrementa
  la generación; un handle stale no resuelve.
- Almacenamiento de entidades vivas: **sparse set de entidades** (dense + sparse) para O(1) existencia.

### 4.2 Componentes
- **POD** para los data-driven (LocalTransform, WorldTransform, MeshRendererData, CameraData, LightData,
  RigidBodyData, ColliderData, AudioSourceData, …). Sin herencia, sin virtuals.
- **Reflection**: cada componente registra metadata (nombre, tamaño, layout, serialize/deserialize JSON).
  Base del inspector, serialización y del tier avanzado futuro.
- **HybridComponent**: componente que lleva un objeto OOP boxeado (con lifecycle `OnAwake/OnStart/
  OnUpdate/OnDestroy`), patrón Unity DOTS adaptado — para los componentes con lógica que aún no
  migramos a sistemas, y para los `ScriptComponent` futuros.
- **Registro por `type_index`** (compile-time id, O(1)) + reflection runtime (por id → metadata).
  Mata los `dynamic_cast` lineales actuales.

### 4.3 Pools (sparse set)
- Un pool por tipo: `dense vector<T>` + `sparse[entity] → index`. Add O(1), remove = swap-and-pop O(1).
- **Máscara de componentes por entidad** (bitset) para query matching y grupos.
- `static_assert` de pod-ness, alineación SIMD (16 bytes mínimo para vec4, 64 para cache line).

### 4.4 Owned groups SoA + journal de cambios estructurales
- Grupo = **SoA** (structure-of-arrays): columnas contiguas por campo (p.ej. todos los `position.x`
  juntos) → SIMD natural (procesar 4/8/16 floats por instrucción).
- Cada grupo declara: *fuente* (componentes que lo alimentan) + *layout* (campos en qué orden).
- El **journal de cambios** registra: `(entidad, tipo de cambio)` al add/remove componente, crear/
  destruir entidad, reparent. El grupo **aplica el journal** (insert/remove row) en un sync point.
- Hot paths objetivo (los primeros grupos):
  - `Renderables` (MeshRenderer + WorldTransform + Camera-visible)
  - `Transforms` (LocalTransform + Parent + WorldTransform)
  - `PhysicsBodies` (RigidBody + Collider + WorldTransform)
  - `Audio3D` (AudioSource espacial + WorldTransform)
  - `Sprites` (SpriteRenderer + WorldTransform)

### 4.5 Query cache
- Query = `With{Tags...}` / `Without{...}` + opcional filtro por relación.
- Se matchea contra los grupos/pools; resultado cacheado (lista de entidades o punteros de columnas).
- Invalidación por journal (solo si el cambio toca los tipos del query).

### 4.6 Systems pipeline
- Sistemas = free functions (o structs con `Update`), declaran **acceso read/write a tipos**.
- Fases: `FixedUpdate` (física, determinista) → `Update` (gameplay) → `Render` (construcción de
  draw commands). Orden entre sistemas del mismo stage = declarado (topológico).
- **Command buffer**: los cambios estructurales desde un sistema se encolan (diferidos) y se aplican en
  sync points — permite iteración segura y paralela (patrón EntityCommandBuffer de Unity / Commands de
  Bevy). Imprescindible para no invalidar iteradores a mitad del loop.
- Scheduler paralelo en Fase 2 (§6).

### 4.7 Transform (crítico — conserva lo que ya escribimos)
- `LocalTransform` (pos/rot/scale POD) + `WorldTransform` (matriz + world pos/rot/lossy scale).
- **Propagación por dirty-frontier**: al mutar un local o al reparent se marca el subtree; el sistema
  de transform re-computa SOLO la frontera sucia, top-down, en los grupos SoA.
- **CONSERVAR el lossy-preserve exacto** de `Transform::SetWorldScale` (largos de columnas de
  `padreWorldMatrix × rotaciónLocalDelHijo` + guard epsilon) — el reparent con `worldPositionStays`
  debe seguir siendo exacto (mejor que Unity).
- SIMD: multiplicación de matrices 4×4 por filas (SSE/NEON 4-lane; AVX 2 matrices a la vez).
- El bridge expone la misma API `SetWorldPosition/Rotation/Scale/SetParent(worldPositionStays)`.

### 4.8 Hierarchy tree unificado
- Arrays compactos: `parent[]`, `firstChild[]`, `nextSibling[]`, `prevSibling[]` (o listas de hijos por
  nodo si el patrón de acceso lo pide). Los nodos son entity ids.
- `AddChild`: O(1). `GetChildren`: O(#hijos). `SetParent(worldPositionStays)`: mover el subtree en el
  tree (O(1) el nodo raíz) + marcar dirty del subtree + journal. `RemoveChild`: O(1).
- El tree NO duplica datos de componentes: solo estructura. Es lo que alimenta el hierarchy panel,
  la serialización y el render order.

---

## 5. SIMD-first en todas las plataformas

Principio: **todo layout es SoA por diseño** (no "optimizar después"). Los hot loops se escriben contra
operaciones vectoriales del módulo Math (regla de `Mathf.h`: el SIMD va wrappeado en
`engine/include/LeirEngine/Math/` — nunca intrínsecos sueltos fuera del módulo Math).

| Plataforma | ISA vectorial | Notas |
|---|---|---|
| Windows / Linux x64 | **SSE4.2** (baseline), **AVX**, **AVX2**, **AVX-512** (detectado en runtime + dispatcher por función) | Transform, física, render list, audio DSP |
| macOS (Apple Silicon) | **NEON** (128-bit, `float32x4_t`), futuras SVE/SVE2 | Lo mismo vía wrapper |
| WebAssembly | **SIMD128** (`v128`) — es la extensión estándar de wasm | El motor web sigue **single-thread** (decisión 2026-08-17, ver AGENTS.md), pero SIMD128 aplica a todos los hot loops; `relaxed-simd` opcional |
| Android / iOS (futuro) | **NEON** (ARMv8-A 64-bit) | Mismo wrapper que macOS |
| RISC-V (futuro) | **V** (vector extension) | Reservado |

**Dispatcher**: en x86 se detecta AVX/AVX-512 en runtime (`__cpuid`) y se elige la variante por función
(patrón `glm` no lo hace bien; nosotros sí). En ARM se compila por target.

**Targets de vectorización concretos**:
- Transform propagation: matrices 4×4 (4 filas × 4 floats = 1 lane SSE/NEON; AVX = 2 matrices).
- Build de draw commands (matriz de mundo × malla) en el render list.
- Physics sync (pos/rot) y audio DSP (SoLoud ya optimiza; nuestros puentes no deben romperlo).
- Serialización binaria (memcpy vectorizado de columnas SoA).
- Queries (bitmasks con `AND`/`ANDNOT` vectorial sobre máscaras de componentes).

---

## 6. Multithreading (Fase 2 — diseñado desde ahora)

- **Job system** propio (thread pool + work-stealing) o reutilizar el modelo de Jolt (ya multithread).
- **Scheduler de sistemas por dependencias de acceso**: cada sistema declara read/write de tipos →
  grafo → ejecución paralela (estilo Bevy). Sin conflicto de acceso, corren en paralelo; con conflicto,
  dependencia.
- **Sync points**: principio de frame, fin de FixedUpdate, fin de Update, antes de Render. El command
  buffer se aplica en sync points.
- **Determinismo**: FixedUpdate con timestep fijo (ya existe 1/60 en PhysicsWorld) y orden estable para
  simulación replicable; Update no determinista (como todo el mundo).
- Web: single-thread (decisión documentada) — el scheduler degrada a ejecución secuencial ordenada.
- Los pools sparse-set son **single-thread en el edit** (nadie más los toca) y **leídos en paralelo** por
  los workers (columnas SoA de solo lectura); las escrituras van por command buffer.

---

## 7. Bridge CoreObject / Scene (API pública inmutable)

```cpp
CoreObject* cube = scene.CreateObject3D("Cube");        // crea entity + tree node + handle
cube->AddChild(light);
cube->SetParent(other, true);                           // worldPositionStays (lossy exacto)
for (CoreObject* c : cube->GetChildren()) { ... }
auto& mr = cube->AddComponent<MeshRenderer>(mesh, mat); // → pool/group + reflection
cube->GetComponent<MeshRenderer>();
```

- `CoreObject` = handle delgado: entity id + puntero al World/Scene. Métodos delegan al tree + ECS.
- `Scene` = dueña del tree + world ECS + caches; `CreateObject3D/2D`, `DestroyObject`, `MoveObject`
  (orden de raíces, ya existente) operan sobre el tree y propagan al ECS vía journal.
- La familia (Object3D/Object2D/UINode) pasa de `dynamic_cast` a **tag componentes** (`Tag3D`,
  `Tag2D`, `TagUI`) → el hierarchy panel filtra por tag, no por RTTI.
- El render/picking/física/audio usan los **groups/query**, no `GetObjects()+GetComponent` scans.

### Estrategia del Bridge: B primero (prueba) → A después (migración definitiva)

**Decisión (2026-08-28)**: el ECS interno ya está completo (entity/pools/groups/journal/systems/
tree/transform). El Bridge se hace en DOS etapas para no romper el editor: primero la **opción B**
(prueba del seam con `ECSScene`, 100% aditiva, riesgo nulo para el editor actual), y **una vez que
ande**, la **opción A** (`CoreObject` → handle del ECS, la migración definitiva). El código de B que
quede obsoleto tras A se BORRA (código limpio, listado abajo).

#### Etapa B — `HybridComponent` + `ECSScene` de prueba (aditivo, el editor actual NO cambia)

- [x] **`HybridComponent<T>`** (`ECS/HybridComponent.h`): componente ECS que boxea un objeto OOP
      `Component` (`std::unique_ptr<T>`), **move-only** (moves explícitos porque el dtor declarado
      suprime los implícitos — `TypedPool` soporta emplace/move/pop), dtor del box llama `OnDestroy()`
      (se dispara al remover el componente o destruir la entidad). Helpers `World::AddHybrid<T>(e, args...)`
      (one-per-type, devuelve `T&` vivo) y `World::GetHybrid<T>(e)`. Verificado en `ECSTest`
      (boxeo + instancia viva + one-per-type + iteración vía `OwnedGroup` + destroy destruye el box).
- [x] **`ECSScene`** (`Scene/ECSScene.{h,cpp}`) implementa `ISceneStorage` (el seam de Fase 0):
      - `World` + `HierarchyTree` + `TransformSystem` + tags de familia (`Tag3D`/`Tag2D`, `ECS/Tags.h`).
      - `CreateObject3D/2D`: crea entity + `LocalTransform` (vía `TransformSystem::SetLocal`, marca dirty)
        + tag de familia + node del tree, y devuelve un `CoreObject`-handle OOP (ver nota abajo).
      - `DestroyObject`/`MoveObject`: operan sobre `m_Objects` (root order) + destruyen la entity/tree.
      - `SyncStructure()` (en `OnUpdate`): reconcilia el ECS tree + `LocalTransform` con la jerarquía
        OOP (DFS desde roots sin padre), corriendo el `TransformSystem` y **escribiendo los WorldTransform
        del ECS de vuelta a los handles** → `GetLocalToWorldMatrix()` devuelve el resultado del ECS.
      - `GetRenderables/GetCameras/GetLights`: por componentes OOP de los handles por ahora (la cache
        rebuilda por acceso — Etapa A los pasa a grupos ECS + HybridComponent).
      - Verificado en `ECSTest` (**ALL PASS**): tags de familia, tree espejo, world ECS == world OOP de
        referencia, **lossy-preserve reparent sin deformación por ECS**, renderables/GetObjects/FindByUUID.
      Nota honesta: en Etapa B los handles son `Object3D` OOP reales (componentes + sync de transform por
      frame, patrón "dos mundos con capa de sync" de la industria); el `ECSScene` de B se BORRA al hacer
      Etapa A (ver lista de limpieza abajo).
- [ ] **Handle provisional** para B: un `CoreObject` mínimo cuyo `GetTransform()` lee/escribe
      `LocalTransform`/`WorldTransform` del ECS (facade) y cuyo `GetComponent<T>` busca
      `HybridComponent<T>`. NO es el handle final (A); es solo para que el renderer/picking prueben B.
- [ ] **Prueba de B**: un demo/test que crea la escena por `ECSScene` y la renderiza con el
      `RenderPipeline` real (que ya consume `ISceneStorage`) → jerarquía + transform + renderables
      por ECS, verificado visualmente y en `ECSTest`.
- [ ] Resultado: **el seam queda probado de punta a punta** sin tocar la API ni el editor actual.

#### Etapa A — `CoreObject` → handle del ECS (migración definitiva)

- [ ] Reescribir `CoreObject`: eliminar `m_Transform`, `m_Children`, `m_Components`; guardar
      `Entity` + `World*`/`Scene*`. `AddChild/GetChildren/SetParent/GetTransform/AddComponent/
      GetComponent/RemoveComponent` delegan al tree + pools + HybridComponent.
- [ ] `Scene` pasa a ser la implementación ECS (funde lo aprendido en `ECSScene`); `ECSScene` se
      BORRA (ya no hay dos implementaciones).
- [ ] `GetTransform()` → facade sobre `LocalTransform`/`WorldTransform` con la semántica exacta actual
      (lossy-preserve, worldPositionStays, guard epsilon) — ya portada al `TransformSystem`.
- [ ] `AddComponent<T>` → `HybridComponent<T>` boxeado (devuelve `T&` vivo, one-per-type).
- [ ] El hierarchy panel filtra por **tags** (`Tag3D/Tag2D/TagUI`) en vez de `dynamic_cast`.
- [ ] Render/picking/física/audio sobre los **groups** del ECS.
- [ ] `Entity` generacional: `CoreObject` valida con `IsAlive` (handle stale seguro).

#### Código de B que se BORRA al completar A (código limpio)

- `ECSScene` (y su `.h/.cpp`) — la implementación ECS de prueba queda absorbida por `Scene`.
- El "handle provisional" de B (si quedó como clase separada) — reemplazado por el `CoreObject` real.
- Cualquier helper temporal del demo/test de B que no aporte a la API final.
- NO se borran: `World`, `TypedPool`, `OwnedGroup`, `HierarchyTree`, `TransformSystem`,
  `SystemPipeline`, `CommandBuffer`, `HybridComponent`, `Entity` — son el motor definitivo.

---

## 8. Atoms (prefabs) — ver `TODO_ATOM.md`

- Un Atom = **subárbol serializado `.atom`** (JSON vía reflection). Instanciar = **deep-copy** del
  subárbol al scene tree + alta de entidades en el ECS (independiente por instancia, semántica Unity).
- Nested atoms: el subárbol puede contener referencias a otros `.atom` (guid) → instanciación recursiva.
- Isolate / Open Separately / Unpack: operaciones del editor sobre el tree, sin tocar el ECS más allá
  del journal.
- El tree unificado hace los atoms triviales: serializar = recorrer el subárbol + volcar componentes
  por reflection.

---

## 9. Serialización (Fase P3, desbloqueada)

- `.scene2D/.scene3D/.uidoc` y `.atom`: JSON vía reflection de componentes (nlohmann_json ya está).
- `.mdata` al lado del asset (estilo Unity/Godot) para metadatos del editor.
- Binario (cereal) a futuro para carga rápida: volcado directo de columnas SoA (memcpy vectorizado).
- Los **ids persistentes** (UUID) viven como componente/atributo del tree node (no del índice del ECS,
  que es efímero).

---

## 10. Fases y checkboxes

### Fase 0 — Refactor data-oriented del código actual (pre-requisito; ejecutar YA)
- [x] **Fix render que solo itera roots** (`RenderPipeline.cpp:244`): un MeshRenderer en un hijo no se
      dibuja. → el renderable cache recorre todo el árbol. (Hallazgo del modelo real: `m_Objects` contiene
      TODOS los objetos —hijos incluidos—, así que el render viejo sí dibujaba hijos pero en orden de
      creación; la cache DFS desde roots sin padre da el orden de jerarquía correcto.)
- [x] **Registro de componentes por `type_index`** (`CoreObject.h`): matar `dynamic_cast` en
      `GetComponent/RemoveComponent` → O(1). Se mantiene `m_Components` (vector) para el orden de
      OnUpdate con un mapa lateral `type_index → índice`. Semántica **one-component-per-type**
      (Unity/Godot): `AddComponent<T>` con el tipo ya presente devuelve la instancia viva.
- [x] **Caches de escena**: `m_Renderables`, `m_Cameras`, `m_Lights` reconstruidas lazy cuando cambia
      un componente/objeto. Hook: `CoreObject::NotifyStructuralChange()` (definido en el `.cpp`, donde
      `Scene.h` sí está completo) llamada desde `Add/RemoveComponent` + `SetParent`/`InsertChildAt`;
      `CreateObject3D/2D`, `DestroyObject`, `MoveObject` también marcan dirty. Rebuild = DFS desde
      **roots sin padre** (regla del HierarchyPanel: `m_Objects` guarda todos, no solo roots). Render +
      picking usan las caches → mata el O(N×dynamic_cast) por frame.
- [x] **Aislar el almacenamiento de Scene** detrás de una interfaz: nuevo `ISceneStorage` (Scene/…)
      con las operaciones estructurales, queries y caches. `Scene` la implementa; `RenderPipeline`
      recibe `ISceneStorage*` (el editor/ejemplos pasan `Scene*` por upcast). El ECS de Fase 1
      implementará el mismo contrato sin tocar la API pública.
- [x] Tests: build limpio + ctest 2/2 + smoke test (workflow AGENTS.md) + test standalone
      (`leir_fase0_test.cpp`: cache incluye hijos/hojas profundas, invalidación por add/remove/reparent,
      one-per-type sin duplicados → ALL PASS).

### Fase 1 — Núcleo ECS custom (después de estabilizar P1 del editor)

**Estado**: núcleo base implementado y testeado (ctest 3/3). El módulo vive en
`engine/include/LeirEngine/ECS/` + `engine/src/ECS/` y es 100% independiente del OOP actual (invisible).

- [x] **`Entity` generacional + allocator** (`ECS/Entity.h`, `World` en `World.cpp`): handle
      `{index, generation}`, índice 0 reservado como null entity (estilo EnTT), free-list de reciclaje con
      bump de generación → un handle stale jamás resuelve a una entidad reutilizada. `Destroy` limpia los
      componentes del índice (por si se recicla). `Create/Destroy/IsAlive` + journal.
- [x] **Registro de tipos por `type_index`** (`World::ComponentType<T>`): asigna un `typeId` entero
      secuencial + metadata `{name, size, align}` (semilla de la reflection; el JSON es un paso de Fase 3).
- [x] **Pools sparse-set por tipo** (`ECS/ComponentPool.h`, `TypedPool<T>`): dense contiguo (SoA-ready) +
      sparse `entity→dense`; add/remove O(1) swap-and-pop sin migración. One-component-per-type por
      entidad (Unity/Godot). La máscara por entidad se implementa vía los sparse arrays (O(1)); el
      bitset explícito se decide en el paso de query cache si aporta.
- [x] **Journal de cambios estructurales + sync points** (`ChangeRecord`, `GetJournal/ClearJournal`,
      `GetChangeVersion`): version monótona + registros de create/destroy/component add/remove; los
      owned groups y query caches (siguiente paso) lo consumen para sync incremental.
- [x] **Owned groups SoA / query cache** (`ECS/OwnedGroup.h`, header-only): grupo cacheado por el journal —
      el conjunto ordenado de entidades que poseen TODOS los `Ts`, mantenido incrementalmente vía
      `Sync(world)` (antes de `ClearJournal`). `ForEach` = O(miembros) **sin checks de membership por
      entidad** (la membresía está cacheada) y **lee datos vivos de los pools** (siempre consistente con
      `World::Get/Add/Remove`). Cubre la "query cache With" para firmas fijas; los grupos específicos
      (Renderables/Transforms/Physics…) y la alineación SoA para SIMD llegan con la migración de
      componentes y la Fase 2. Verificado en `ECSTest` (crecer/encoger al cambiar membresía, drop por
      destroy, datos vivos).
  - **NOTA HONESTA — pendientes OBLIGATORIOS (profesional, no opcionales)**:
    - [ ] **Orden de filas ESTABLE** ante remociones: hoy `Reconcile` usa swap-and-pop, que cambia el
          orden de `m_Members`. Para render (z-order), serialización determinista y determinismo de
          simulación el orden debe ser estable → remoción con "tombstone" o reemplazo por el último con
          `m_Members` como linked-list/binary-heap con índice libre, y `m_RowOf` para O(1) lookup del
          slot. No dejar swap-and-pop como comportamiento permanente.
    - [ ] **Alineación SoA por campo (SIMD)**: hoy el grupo itera filas que leen los pools (datos vivos,
          correcto pero acceso indirecto por fila). La Fase 2 debe convertir los hot groups en columnas
          contiguas por campo (p.ej. `posX[]/posY[]/posZ[]`) con filas alineadas 16/64 bytes para
          SSE/NEON/SIMD128. Es requisito de perf, no un extra.
    - [ ] **Ownership de almacenamiento**: decidir entre (a) grupo como caché read-only (hoy, con
          re-sync al add de fila) y (b) grupo dueño del pool (una sola fuente, sin copia) — el patrón
          enTT de sección "owned" al frente del dense con reordenación. La opción (b) elimina la
          duplicación y es la que rinde a escala; evaluarla al migrar los componentes.
- [x] **Systems pipeline básico** (`ECS/System.h` + `System.cpp`): `ISystem` (nombre + `Update(dt)`),
      `SystemPipeline` con fases **FixedUpdate → Update → Render** en orden de registro (el orden
      declarado ES la dependencia para v1; el scheduler paralelo por read/write llega en Fase 2).
      **`CommandBuffer`** (deferred structural changes — patrón EntityCommandBuffer/Bevy Commands):
      sistemas encolan `Destroy`/`Add<T>(valor)`/`Remove<T>` mientras iteran y el buffer hace
      `Replay(world)` en el sync point (no invalida iteradores). Verificado en `ECSTest` (MoveSystem
      mueve Position por Velocity×dt en Update; ExpireSystem encola destroy diferido; add/remove
      diferidos con datos).
- [x] **Transform system** (`ECS/TransformSystem.{h,cpp}` + PODs `LocalTransform`/`WorldTransform`):
      computa `WorldTransform` desde `LocalTransform` + el tree, top-down, **dirty-frontier**
      (solo subtrees mutados; el padre se asegura limpio antes que el hijo — recursión `EnsureClean`).
      `SetParent(entity, parent, worldPositionStays)` con el **lossy-preserve exacto** (divide por los
      largos de columnas de `parentWorld · localRot` + guard epsilon para ejes a escala 0) y guard
      `IsFinite` de la inversa del padre singular (evita NaN). Verificado en `ECSTest` (hereda pos del
      padre con stays=false; rot+scale → world identity con stays=true; mover padre propaga al hijo;
      eje a escala 0 finito).
- [x] **Hierarchy tree unificado** (`ECS/HierarchyTree.{h,cpp}`): adjacency arrays
      parent/firstChild/lastChild/nextSibling/prevSibling + depth por índice de entidad, O(1)
      `GetParent/GetChildren`, `SetParent` con detach+append y **guard de ciclos**, `ClearEntity`
      (detach + promueve hijos a roots) para el destroy. Verificado en `ECSTest`.
- [ ] **Bridge** en dos etapas (ver §7 "Estrategia del Bridge"): **Etapa B** `HybridComponent` +
      `ECSScene` de prueba (aditivo, riesgo nulo) → **Etapa A** `CoreObject` → handle del ECS
      (migración definitiva; se borra el código de prueba de B). Tags de familia `Tag3D/Tag2D/TagUI`.
- [ ] Migrar MeshRenderer/Camera/Light/Sprite/RigidBody/Collider/Audio a data + sistemas.
- [ ] Render pipeline sobre Renderables group (+ fix de orden de dibujo por jerarquía).
- [ ] Tests de regresión: el editor y los demos deben verse/andar idénticos.

### Fase 2 — SIMD + Multithreading
- [ ] Wrappers SIMD en el módulo Math (SSE/AVX + dispatcher x86, NEON ARM, SIMD128 wasm).
- [ ] Transform propagation SIMD (4×4 por lanes).
- [ ] Render list / draw command build vectorizado.
- [ ] Job system + scheduler de sistemas por dependencias de acceso.
- [ ] Command buffer aplicado en sync points (paralelo seguro).
- [ ] Benchmarks: fps / frame time / cache misses por plataforma (targets en §11).

### Fase 3 — Tier advanced ECS (opcional, para power users)
- [ ] Exponer el `World` ECS público (crear entidades/sistemas directamente, estilo Unity DOTS).
- [ ] El camino OOP sigue siendo el default documentado.

---

## 11. Benchmarks y targets

| Métrica | Target |
|---|---|
| Iterar render list de 100k MeshRenderer | < 1 ms (SoA + SIMD) |
| Transform propagation de 100k nodos (10k dirty) | < 1 ms |
| Add/remove componente | O(1), < 50 ns (sin migración) |
| Reparent de subtree de 1k | < 0.1 ms |
| Spawn 10k entidades | < 10 ms (batch, vía command buffer) |
| Frame time editor (escena demo actual) | igual o menor que hoy (sin regresión) |
| Web (wasm single-thread + SIMD128) | los mismos algoritmos, coste por entidad comparable |

Instrumentación: XConsole `[ECS]` Trace/Debug (nunca Info en per-frame — regla `TODO_UI_EVENT_FLOOD.md`),
stats addon propio (entidades, archetypes/grupos, migraciones, hits de cache) en el `UIDebugOverlay`.

---

## 12. Decisiones y notas

- El ECS es **100% propio** (sin flecs/entt). Motivos: control total del layout, SIMD por plataforma,
  integración con el tree unificado, sin dependencias nuevas, regla de no reinventar aplica a librerías
  grandes pero el ECS es el corazón del motor y queremos que sea exactamente a nuestra medida.
- Regla Mathf: todo SIMD va wrappeado en `engine/include/LeirEngine/Math/` (nunca intrínsecos sueltos
  fuera del módulo Math). Reutiliza `Mathf::`, `Matrix4x4`, `Quaternion`, `Vector3/4` — internamente
  optimizables sin romper el contrato.
- El editor sigue siendo single-thread para UI (como Unity/Unreal/Godot); el paralelismo vive en los
  sistemas del mundo y en las caches — el UI nunca se toca desde workers.
- Determinismo solo donde importa (FixedUpdate/física). El resto es best-effort.
- El bridge debe conservar las semánticas ya verificadas: `SetParent(worldPositionStays)` con
  lossy-preserve exacto (2026-08-28) y el guard epsilon contra NaN con escalas a cero.