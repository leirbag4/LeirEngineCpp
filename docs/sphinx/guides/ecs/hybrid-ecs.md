# Hybrid ECS — arquitectura interna

Cómo funciona nuestro ECS por dentro: entidades generacionales, pools sparse-set, journal, queries de
orden estable, jerarquía, transforms con lossy-preserve, SIMD, scheduling paralelo, física/audio a
data — todo explicado con código y gráficos.

## El viaje: de OOP a data-oriented

LeirEngine nació con la arquitectura clásica OOP (`CoreObject` con `Transform`, árbol de hijos,
componentes tipo Unity). Cada objeto era una caja (unique_ptr) y `GetComponent` hacía casts lineales.
Para escalar a miles de entidades se necesitaban **datos contiguos, procesamiento en lotes y
paralelismo sin carreras**.

El motor fue migrado al ECS en fases, con una regla de oro: **la API amigable no cambia**.
`CoreObject::AddChild`, `SetParent`, `AddComponent<T>`, `GetComponent<T>` siguen funcionando igual —
solo que por debajo delegan al ECS.

```text
CoreObject / Scene (OOP)  →  facade → delega  →  World / Entity / Pools  →  SIMD + Jobs
```

El resultado es un diseño híbrido: componentes de datos puros (rápidos) y componentes OOP boxeados
conviven en el mismo almacenamiento.

## Arquitectura en capas

```text
┌─────────────────────────────────────────────────────────────────────┐
│ API pública OOP — Scene · CoreObject · Transform · AddComponent     │
│ API pública ECS — World · Entity · OwnedGroup · ISystem · Pipeline  │
├─────────────────────────────────────────────────────────────────────┤
│ Sistemas de dominio — PhysicsSyncSystem · AudioSyncSystem ·         │
│                       TransformSystem · RenderPipeline              │
├─────────────────────────────────────────────────────────────────────┤
│ Núcleo ECS — World (journal, hybrids) · TypedPool · SoAPool ·       │
│              OwnedGroup · HierarchyTree · SystemPipeline · CmdBuf   │
├─────────────────────────────────────────────────────────────────────┤
│ Infraestructura — JobSystem · Mathf (Simd4f/SoA) · XConsole ·       │
│                  Settings                                          │
└─────────────────────────────────────────────────────────────────────┘
```

Toda la capa OOP es un facade sobre el mismo núcleo ECS — una sola fuente de datos. Los headers
públicos viven en `engine/include/LeirEngine/` con `LEIR_API`; los templates (pools, grupos) son
header-only (dllexport de templates rompe el link en MSVC).

## Entidades y generaciones

Una `Entity` es un handle de 2 campos: `{ uint32_t index; uint32_t generation; }`. El índice 0 está
reservado como entidad nula. Al destruir una entidad, su índice vuelve a una free-list y su generación
sube — cualquier handle viejo deja de resolver.

```text
index:  0   1   2   3   ...
gen:    0   1   1   2   ...      Destroy(3) → gen 2 → el handle viejo {3,1} no resuelve
```

```cpp
struct Entity {
    uint32_t index = kNullIndex; // kNullIndex = 0
    uint32_t generation = 0;
    explicit operator bool() const { return index != kNullIndex && generation != 0; }
};
```

Ventajas: sin punteros colgantes, handles baratos (8 bytes). Costo: un salto de indirección por
acceso — que los pools resuelven con sparse-set.

## Almacenamiento: TypedPool (AoS) y SoAPool (columnas)

### TypedPool<T> — sparse-set AoS

Un pool por tipo de componente: un dense contiguo (cache-friendly) y un sparse (`entityIndex →
denseIndex`). Add/remove son O(1) con swap-and-pop.

```text
entity:  0   1   2   3   4
sparse:  -   1   0   -   2
                ↓
dense[0]: entity2  data...
dense[1]: entity1  data...
dense[2]: entity4  data...
```

```cpp
T*  Get(uint32_t ei);
T&  Add(uint32_t ei);          // one-per-type; devuelve el existente si ya está
void RemoveFromEntity(uint32_t ei); // swap-and-pop
T*  Data();                    // dense contiguo (SoA-ready)
```

### SoAPool<T> — columnas por campo

Cada campo es una columna contigua de floats (lista para SIMD, sin gathers). Sparse + swap-and-pop
igual, pero el "dense" son columnas paralelas.

```text
col x:  0   1   2   3     +1  →  1  2  3  4
col y:  0   0   0   0     (Mathf::SimdAddFloats, 4 a la vez)
col z:  0   0   0   0
```

```cpp
SoAPool<Position> cols;
cols.Set(ei, pos);          // add-or-update (sparse O(1))
cols.Remove(ei);
bool ok = cols.Get(ei, out);  // materializa (gather por columnas)
const float* x = cols.Col(0); // columna contigua → SIMD
```

:::{note}
SoAPool **complementa** a TypedPool — no lo reemplaza. La API `World::Get<T> → T&` necesita el objeto
completo (AoS); el SoAPool es para sistemas que iteran una columna en bulk (cull, partículas, crowds).
:::

## Journal y OwnedGroup (orden estable)

El `World` registra cada cambio estructural en un journal (`{ entityIndex, typeId, kind }`). Las
queries son conjuntos derivados del journal: `Sync()` las reconcilia y `ForEach()` itera sin chequeos.

```text
world.Add<T>(e) → Journal + ChangeVersion++ → group.Sync(world) → ForEach (O(miembros))
```

### Orden estable (tombstones)

Remover un miembro NO reordena: solo deja un tombstone (row muerta) reutilizable. Así el orden de los
vivos nunca cambia — crítico para z-order, serialización y simulación determinista.

```text
row:     0   1   2   3            remove(B) →
member:  A   B   C   D            row:     0   1   2   3
alive:   1   1   1   1            member:  A  (B†)  C   D
                                  alive:   1   0   1   1
```

## HierarchyTree — scene graph compacto

La jerarquía es un grafo de escena compacto por índice de entidad: arrays de adyacencia (parent /
firstChild / lastChild / nextSibling / prevSibling / depth). O(1) en los getters, `SetParent` con
detach+append y guard de ciclos.

```text
index:       0    1    2    3    4    5
parent:      -    -    1    1    3    -
firstChild:  2    2    -    -    5    -
nextSibling: -    3    -    -    4    -
depth:       0    0    1    1    2    -
```

```cpp
tree.SetParent(child, parent);  // detach + append + guard de ciclos
tree.GetParent(ei); tree.GetFirstChild(ei); tree.GetNextSibling(ei); tree.GetDepth(ei);
tree.ClearEntity(ei); // destroy: detach + promueve hijos a roots
```

## TransformSystem — dirty-frontier + lossy-preserve + SIMD

### Dirty frontier

Los `LocalTransform` son authored; los `WorldTransform` se computan top-down solo para sub-trees
mutados. Cada `SetLocal` marca el subtree dirty; `Update()` recorre la frontera garantizando que el
padre se limpia antes que el hijo.

```text
SetLocal(e) → MarkSubtreeDirty(e) → Update(): EnsureClean(parent) → ComputeWorld → WT fresco
```

### Lossy-preserve exacto (mejor que Unity)

Al reparentar con `worldPositionStays=true`, el local se re-deriva para preservar el world: la escala
local se divide por el largo de las columnas de `parentWorld · localRot`. Esto preserva la escala
lossy exacta incluso con padre rotado + escalado no-uniforme — Unity divide por el lossy del padre y
aplasta al hijo en ese caso. Guard epsilon (`1e-8`) para ejes a escala 0.

```cpp
const Matrix4x4 combined = parentWT->worldMatrix * localRotM;
const Vector3 colLen = ComputeColumnLengths(combined);
lt.scale = { scale.x / (colLen.x > 1e-8f ? colLen.x : 1.0f), ... };
```

:::{note}
**Límite honesto (modelo TRS):** si el padre está rotado + escalado no-uniforme, la matriz de mundo
del hijo puede conservar **shear** que el TRS local no puede expresar — limitación compartida con
Unity (que encima deforma la escala). Documentado, no un bug.
:::

### SIMD

El hot path `parentWorld × local` usa `Matrix4x4::MultiplySimd` (splat+FMA por columna, 16 FMA + 4
stores) — 5.6× vs escalar en Release.

## La capa SIMD

Toda la matemática vive en `Mathf`. `Simd4f` son 4 floats con load/store/add/mul/fma/splat/lane/
min/less/or por plataforma:

| Plataforma | Backend | Estado |
|---|---|---|
| x64 Windows/Linux | SSE2 (baseline) | ✅ (AVX/AVX-512 documentado, futuro) |
| ARM64 | NEON `float32x4_t` | ✅ |
| Web (wasm) | SIMD128 `v128_t` | ✅ |
| Otros | escalar | ✅ fallback |

Consumidores: `Matrix4x4::MultiplySimd` (poses), `Mathf::SimdAddFloats` (columna SoA, x3.7),
`Frustum::TestSphere/CullBatch` (frustum cull: /O2 x3.55; batch lane=renderable x1.48 en Debug).

## Sistemas y scheduler paralelo

Cada `ISystem` declara qué tipos lee/escribe (`GetAccess`). El `SystemPipeline` hace scheduling
topológico por niveles dentro de cada fase: dos sistemas conflictan si comparten un tipo y alguno lo
escribe; los de un mismo nivel (independientes) corren en paralelo vía el `JobSystem`.

```text
nivel 0:  MoveSystem (writes Pos)   ExpireSystem (reads Life)   ← en paralelo
nivel 1:  ReadPosSystem (reads Pos)                             ← después de Move
```

El `JobSystem::ParallelFor` usa chunking (cada grab toma 64 índices) — sin chunks el `fetch_add` por
índice hacía el paralelo más lento (0.14×); con chunks 5× más rápido.

## CommandBuffer y sync points

Los cambios estructurales diferidos se encolan mientras se itera; `Replay` los aplica en el sync point
(entre fases, en el hilo del caller). Thread-safe (mutex).

```text
Sistemas paralelos → CommandBuffer (mutex) → Sync point: Replay → World actualizado
```

## Componentes híbridos (data + OOP)

`HybridComponent<T>` boxea un `Component` OOP dentro del ECS. Es move-only (unique_ptr necesita moves
correctos para el swap-and-pop). Al remover la entidad o el componente, el box llama `OnDestroy()` y
des-registra la instancia.

```cpp
world.AddHybrid<Pinger>(npc);
Pinger* p = world.GetHybrid<Pinger>(npc);
for (Component* c : world.GetHybrids()) c->Tick(dt); // driver del lifecycle
```

## Scene = el driver fusionado

`Scene` fusiona el mundo OOP con el ECS: cada `CoreObject` es una entidad backed; su `Transform` es un
facade sobre el `TransformSystem`. El `OnUpdate` orquesta:

```text
StepPhysics → PhysicsSyncSystem → AudioSyncSystem → hybrid Tick → obj->OnUpdate
            → transforms.Update() → SceneGroups.Sync + ClearJournal
```

```cpp
void Scene::OnUpdate(float dt) {
    m_Physics.Update(...);
    m_PhysicsSync.Update(m_World, m_Transforms);
    m_AudioSync.Update(m_World);
    for (Component* c : m_World.GetHybrids()) c->Tick(dt);
    for (CoreObject* o : m_Objects) o->OnUpdate(dt);
    m_Transforms.Update();
    m_RenderGroup.Sync(m_World); m_SpriteGroup.Sync(m_World);
    m_CameraGroup.Sync(m_World);  m_LightGroup.Sync(m_World);
    m_World.ClearJournal();
}
```

El render corre después (OnRender) leyendo los grupos journal-fresh — nunca stale.

## Render data-oriented (cull SIMD)

El `RenderPipeline` consume `ISceneStorage` (grupos journal-synced) y arma el render list en una
pasada: cull de frustum (6 planos, SIMD), copy SIMD de la world matrix, y draw command build en
columnas por campo. Los draws fuera del frustum se saltan completos.

```text
Renderables group → Frustum::CullBatch (columnas, 4/lote) → renderList (columnas) → Draw
```

La esfera de cull es conservadora (radio local de la malla × max-axis del worldScale) — sin falsos
negativos. Paridad SIMD==escalar verificada por-elemento en 100k esferas.

## Física y audio a data

- **PhysicsSyncSystem**: `RigidBody`/`Collider` son POD en pools; el sistema crea el body Jolt lazy y
  sincroniza por tipo (Dynamic: Jolt→mundo; Kinematic: mundo→Jolt; Static: nada). El dtor del
  `RigidBody` destruye el body.
- **AudioSyncSystem**: `AudioSource` es move-only (el SoundId se transfiere al mover y se libera en el
  dtor). `OnAwake` crea el source; el lifecycle (play-on-awake + sync 3D) lo maneja el sistema leyendo
  la pose mundial. El `AudioListener` es un marcador.

Ambos corren en `Scene::OnUpdate`. Los headers públicos jamás exponen tipos Jolt/SoLoud.

## Escala y benchmarks

Medidos en `ECSTest` (informativos; Debug x64 — los targets son /O2; en Debug todo mide ~10-30× peor):

| Métrica | Target (/O2) | Medido (Debug) |
|---|---|---|
| Spawn 10k entidades | < 10 ms | **4.5 ms** ✅ |
| Add/remove componente (100k) | < 50 ns | ~1 µs/op (O(1), Debug) |
| Iterar render list 100k | < 1 ms | ~10 ms (Debug) |
| Transform propagation 10k dirty | < 1 ms | 45.7 ms (Debug; full 100k = 469 ms) |
| Reparent subtree 1k | < 0.1 ms | **0.06 ms** ✅ |
| Frustum cull 100k | — | 0.47 ms (/O2, x3.55 SIMD) |
| mat4x4 multiply SIMD (2M) | — | x5.6 vs escalar (/O2) |
| SoA add 4M floats | — | x3.7 vs escalar |

El frame time del editor es igual o menor que antes del ECS (sin regresión, verificado visualmente).

## Plataformas y CI

- **Desktop**: engine completo, `JobSystemThreadPool` (Jolt multithread), SSE2/NEON.
- **Web (wasm)**: `JobSystem` inline (decisión single-thread — sin SharedArrayBuffer/COOP-COEP), Jolt
  `JobSystemSingleThreaded`, SIMD128, timestep fijo 1/60, stack 16 MB.
- **CI**: 3 OS con MSVC/cl + job Emscripten. Tests: PhysicsTest, SlangExportTest, ECSTest (ctest 3/3).

## Qué sigue

- **AVX/AVX-512 dispatcher** (documentado, futuro): `__cpuid`+`XGETBV`, variantes por TU, punteros a
  función con fallback SSE2. La máquina del dev (i5-1035G1, Ice Lake) tiene AVX2 + AVX-512.
- **Storage SoA por campo en pools concretos** — la primitiva (`SoAPool`) ya existe; se adopta cuando
  un sistema lo necesite (partículas, crowds).

## Glosario

| Término | Qué es |
|---|---|
| `World` | Contenedor: records, pools, journal, hybrids, registro de tipos. |
| `Entity` / Record | Handle generacional `{índice, generación}`. Índice 0 = nulo. |
| `TypedPool` | Pool AoS sparse-set (dense contiguo + sparse índice). O(1). |
| `SoAPool` | Pool por columnas (un campo = una columna float contigua). SIMD. |
| `Journal` | Bitácora de cambios estructurales que alimenta queries. |
| `OwnedGroup` | Query cacheada por journal, orden estable (tombstones). |
| `CommandBuffer` | Cambios diferidos thread-safe, replay en sync points. |
| `SystemPipeline` | Fases Fixed/Update/Render + scheduling topológico paralelo. |
| `JobSystem` | Thread pool (ParallelFor chunked; web inline). |
| `HybridComponent` | Box OOP (Component con lifecycle) dentro del ECS. |
| `HierarchyTree` | Scene graph compacto por arrays de adyacencia. |
| `TransformSystem` | Poses mundiales: dirty-frontier + lossy-preserve + SIMD. |
| `Tags` | Marcadores vacíos (Tag3D/Tag2D/TagUI, Active). |