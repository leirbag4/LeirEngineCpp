# API pública del ECS

La capa de datos orientada a rendimiento del motor, expuesta como API de primer nivel para power
users. Creada de cero, con nombres propios y sin copiar el estilo de Unity.

Existen dos maneras de construir sistemas, y ambas comparten la misma fuente de datos:

- **Camino OOP (default)** → `Scene` · `CoreObject` · `AddComponent`
- **Camino ECS (power users)** → `World` · `Entity` · pools · `OwnedGroup` · `ISystem` · `CommandBuffer`

Esta guía documenta el camino ECS: control directo de entidades, componentes de datos, queries,
sistemas con scheduling paralelo, comandos diferidos y almacenamiento en columnas SIMD — sin pasar por
el facade orientado a objetos.

## El vocabulario (nombres propios)

| Término | Rol |
|---|---|
| `World` | El contenedor: records, campos, pools, journal y registro de hybrids. |
| `Entity` / Record | Handle generacional `{índice, generación}`. Nunca resuelve handles viejos. |
| `TypedPool<T>` | Pool de campos (AoS): filas POD contiguas, sparse-set, add/remove O(1). |
| `SoAPool<T>` | Pool de columnas SoA: una columna contigua por campo, lista para SIMD. |
| `OwnedGroup<Ts...>` | Query cacheada por journal, con orden estable y `ForEach` sin chequeos. |
| `CommandBuffer` | Cambios estructurales diferidos, thread-safe, replay en sync points. |
| `ISystem` | Un paso del simulador con acceso declarado a los campos. |
| `SystemPipeline` | Fases FixedUpdate → Update → Render, scheduling topológico paralelo. |
| `JobSystem` | Thread pool con `ParallelFor` y `Dispatch`/`WaitAll` (en web, inline). |
| `HybridComponent<T>` | Faceta OOP boxeada en el ECS: `Component` clásico con lifecycle dentro del World. |
| `Tags` | Marcadores vacíos (`Tag3D`/`Tag2D`/`TagUI`, `Active`) para filtrar familias. |
| `HierarchyTree` | Scene graph compacto: parent / firstChild / nextSibling / depth por índice. |
| `TransformSystem` | Poses mundiales con dirty-frontier, lossy-preserve y SIMD. |

## World y Record — entidades generacionales

Un `Record` es un handle generacional: al destruir una entidad, su índice se recicla pero la
generación sube, así cualquier handle viejo queda inválido de forma segura. El índice 0 está
reservado como entidad nula.

```cpp
World world;

Entity hero = world.Create();
Entity foe  = world.Create();

world.Destroy(foe);
Entity fresh = world.Create();   // quizá reutiliza el índice, con otra generación
bool alive = world.IsAlive(foe); // false — el handle viejo nunca resuelve
```text

## Campos — componentes de datos (POD)

Un campo es un struct POD tuyo. El World lo guarda en un pool sparse-set (one-per-type por entidad).
Sin macros, sin registros, sin boxes.

```cpp
struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float x = 0, y = 0, z = 0; };

Entity e = world.Create();
world.Add<Position>(e) = { 0, 0, 0 };
world.Add<Velocity>(e) = { 1, 0, 0 };

Position* p = world.Get<Position>(e);  // puntero al POD vivo (AoS)
bool has  = world.Has<Position>(e);
world.Remove<Position>(e);             // O(1), swap-and-pop
```

El `World` mantiene un journal de cambios estructurales que alimenta a las queries — nunca hay que
marcar "dirty" a mano:

```cpp
const std::vector<ChangeRecord>& j = world.GetJournal();
world.ClearJournal(); // el dueño del sync point lo limpia
```text

## Queries — OwnedGroup (orden estable)

`OwnedGroup<Ts...>` es el conjunto ordenado de records que poseen TODOS los campos declarados. Se
mantiene incrementalmente desde el journal: `Sync()` consume los cambios y `ForEach()` itera sin
chequeos por entidad. El orden es **estable** (remover solo deja un tombstone).

```cpp
OwnedGroup<Position, Velocity> movers(&world);
movers.Sync(world);   // consume el journal UNA vez (antes de ClearJournal)
world.ClearJournal();

movers.ForEach([&](Position& p, Velocity& v, Entity e) {
    p.x += v.x * dt;
});
```

:::{tip}
Re-sincronizá tus queries después de cambios estructurales (p. ej. tras un `CommandBuffer::Replay`)
antes de leerlas. Una query stale puede referenciar un record destruido.
:::

## Sistemas — ISystem + SystemPipeline

Un sistema declara qué campos lee y cuáles escribe (`GetAccess`); el `SystemPipeline` usa eso para el
scheduling topológico: dos sistemas que comparten un tipo y alguno lo escribe corren en orden de
registro; los independientes corren en paralelo vía el `JobSystem`.

```cpp
class MovementSystem : public ISystem {
public:
    MovementSystem(World* w, OwnedGroup<Position, Velocity>* g)
        : ISystem("Movement"), m_G(g),
          m_Pos(w->ComponentType<Position>()),
          m_Vel(w->ComponentType<Velocity>()) {}

    std::vector<SystemAccess> GetAccess() const override {
        return { { m_Pos, true }, { m_Vel, false } }; // escribe Position, lee Velocity
    }
    void Update(float dt) override {
        m_G->ForEach([dt](Position& p, Velocity& v, Entity) {
            p.x += v.x * dt; p.y += v.y * dt; p.z += v.z * dt;
        });
    }
private:
    OwnedGroup<Position, Velocity>* m_G;
    uint32_t m_Pos, m_Vel;
};

SystemPipeline pipeline;
pipeline.Add(&moveSys, SystemPhase::Update);
JobSystem jobs;
pipeline.Run(0.016f, dt, &jobs, &cb, &world); // fases + cb replayed + paralelo
```text

Las fases son `FixedUpdate` → `Update` → `Render`. El pipeline aplica el `CommandBuffer` entre fases,
en el hilo del caller.

## Cambios diferidos — CommandBuffer

Nunca destruyas o agregues componentes mientras iterás: encolá en el `CommandBuffer` y aplicá en el
sync point. Es thread-safe (mutex).

```cpp
CommandBuffer cb;
cb.Destroy(e);                  // destrucción diferida
cb.Add<Health>(e, { 50 });      // add con datos
cb.Remove<Velocity>(e);         // remove
cb.Replay(world);               // sync point
```

## Columnas SoA — SoAPool + SIMD

Para sistemas que procesan UN campo en bulk, `SoAPool<T>` guarda cada campo en una columna contigua
de floats — lista para SIMD sin gathers.

```cpp
SoAPool<Position> cols;
for (int i = 0; i < 8; ++i) cols.Add((uint32_t)i, { (float)i, 0, 0 });

std::vector<float> inc(cols.Count(), 0.1f);
Mathf::SimdAddFloats(cols.Col(0), cols.Col(0), inc.data(), cols.Count());

Position p{};
cols.Get(3, p);   // materializar una fila (gather por columnas)
cols.Remove(3);   // swap-and-pop O(1)
```text

## Facets — HybridComponent

Un `HybridComponent<T>` boxea un `Component` clásico (con `OnStart/OnUpdate/OnDestroy`) en el ECS.
Removerlo o destruir la entidad lo destruye (llamando `OnDestroy`).

```cpp
Entity npc = world.Create();
world.AddHybrid<Pinger>(npc);

for (Component* c : world.GetHybrids())
    c->Tick(dt); // OnStart una vez + OnUpdate si está activo
```

Es el puente híbrido: campos de datos (rápidos, SoA/SIMD) y facets OOP (amigables) conviven en el
mismo World.

## Jerarquía y poses — HierarchyTree + TransformSystem

```cpp
HierarchyTree tree;
TransformSystem ts(&world, &tree);

Entity root = world.Create();
ts.SetLocal(root, { { 0, 0, 0 }, Quaternion::Identity(), Vector3::One() });
Entity kid  = world.Create();
ts.SetLocal(kid,  { { 2, 0, 0 }, Quaternion::Identity(), Vector3::One() });
ts.SetParent(kid, root);          // worldPositionStays = true (lossy-preserve)
ts.Update();

WorldTransform* wt = ts.GetWorld(kid);
```text

## Marcadores — Tags

```cpp
world.Add<Tag3D>(obj3d);   // familia 3D
world.Add<Tag2D>(uiObj);   // familia 2D
world.Add<Active>(e) = { false };

size_t n = 0;
world.Each<Tag3D>([&](Tag3D&, Entity) { ++n; });
```

## Consejos y footguns

- Nunca iteres y destruyas en el mismo pase: encolá en un `CommandBuffer` y reaplicá en el sync point.
- Re-sincronizá las queries después de cambios estructurales antes de leerlas.
- El índice 0 es la entidad nula — no lo uses como nodo real de jerarquía.
- Las queries son de orden estable: el orden de los vivos no cambia al remover (tombstones).
- En paths per-frame usá `XConsole::Trace/Debug`, nunca Info/Warning/Error.
- Los benchmarks del motor se miden en Debug (~10-30× más lento por diseño); para números reales,
  compilá con `/O2`.

## El demo — ECSPublicDemo

Un demo renderless que recorre todo el vocabulario (records + campos, query estable, scheduler
paralelo con CommandBuffer, columna SoA con SIMD, facet híbrido Pinger, jerarquía con poses):

```text
& "build\windows-debug\examples\ECSPublicDemo\Debug\LeirEngineECSPublicDemo.exe"
```

Ver también: {doc}`hybrid-ecs` — la arquitectura interna en detalle.