// ECSPublicDemo — Fase 3: the ECS public API (our names, no Unity style).
// Renderless: drives the ECS directly (no Scene/CoreObject facade) and prints.
// Shows: World + Entity (records), data components (POD fields), OwnedGroup
// (stable journal-synced query), SystemPipeline + ISystem (dependency-ordered
// parallel scheduling), CommandBuffer (deferred structural changes replayed at
// sync points), SoAPool (SoA column storage + SIMD), Tags, HybridComponent
// (OOP facet boxed in the ECS), HierarchyTree + TransformSystem (world poses).
#include <LeirEngine/ECS/World.h>
#include <LeirEngine/ECS/OwnedGroup.h>
#include <LeirEngine/ECS/SoAPool.h>
#include <LeirEngine/ECS/System.h>
#include <LeirEngine/ECS/CommandBuffer.h>
#include <LeirEngine/ECS/HybridComponent.h>
#include <LeirEngine/ECS/Tags.h>
#include <LeirEngine/ECS/HierarchyTree.h>
#include <LeirEngine/ECS/TransformSystem.h>
#include <LeirEngine/Core/Component.h>
#include <LeirEngine/Core/JobSystem.h>
#include <LeirEngine/Math/SoA.h>

#include <cstdio>
#include <vector>

using namespace Leir;
using namespace Leir::ECS;

// ---- Data components (Fields): plain PODs — one SoA column each. ----
struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float x = 0, y = 0, z = 0; };
struct Lifetime { float remaining = 1.0f; };

// ---- A hybrid facet: a classic OOP Component boxed inside the ECS. ----
class Pinger : public Component {
public:
    void OnStart() override { printf("    [Pinger] OnStart (once)\n"); }
    void OnUpdate(float dt) override
    {
        m_T += dt;
        if (m_T >= 1.0f) {
            printf("    [Pinger] ping #%d\n", ++m_Pings);
            m_T = 0.0f;
        }
    }
    void OnDestroy() override { printf("    [Pinger] OnDestroy\n"); }
private:
    float m_T = 0.0f;
    int m_Pings = 0;
};

// ---- Systems ----
// Movement: writes Position, reads Velocity. Declared access lets the scheduler
// run it in parallel with systems touching other components.
class MovementSystem : public ISystem {
public:
    MovementSystem(World* w, OwnedGroup<Position, Velocity>* g)
        : ISystem("Movement"), m_G(g),
          m_PosType(w->ComponentType<Position>()), m_VelType(w->ComponentType<Velocity>()) {}
    std::vector<SystemAccess> GetAccess() const override
    {
        return { { m_PosType, true }, { m_VelType, false } };
    }
    void Update(float dt) override
    {
        m_G->ForEach([dt](Position& p, Velocity& v, Entity) {
            p.x += v.x * dt; p.y += v.y * dt; p.z += v.z * dt;
        });
    }
private:
    OwnedGroup<Position, Velocity>* m_G;
    uint32_t m_PosType, m_VelType;
};

// Expire: reads Lifetime, enqueues a DEFERRED destroy via the CommandBuffer
// (never destroys while iterating — iterators stay valid).
class ExpireSystem : public ISystem {
public:
    ExpireSystem(World* w, OwnedGroup<Lifetime>* g, CommandBuffer* cb)
        : ISystem("Expire"), m_G(g), m_CB(cb), m_LifeType(w->ComponentType<Lifetime>()) {}
    std::vector<SystemAccess> GetAccess() const override { return { { m_LifeType, false } }; }
    void Update(float dt) override
    {
        m_G->ForEach([this, dt](Lifetime& life, Entity e) {
            life.remaining -= dt;
            if (life.remaining <= 0.0f)
                m_CB->Destroy(e);
        });
    }
private:
    OwnedGroup<Lifetime>* m_G;
    CommandBuffer* m_CB;
    uint32_t m_LifeType;
};

// Render phase: reads the stable-order query and summarizes — the "render list"
// of the ECS path.
class RenderSummarySystem : public ISystem {
public:
    RenderSummarySystem(World* w, OwnedGroup<Position, Lifetime>* g)
        : ISystem("RenderSummary"), m_G(g), m_PosType(w->ComponentType<Position>()) {}
    std::vector<SystemAccess> GetAccess() const override { return { { m_PosType, false } }; }
    void Update(float) override
    {
        size_t count = 0;
        float sum = 0.0f;
        m_G->ForEach([&](Position& p, Lifetime&, Entity) { ++count; sum += p.x; });
        printf("    [Render] %zu records, sum(Position.x)=%.2f\n", count, sum);
    }
private:
    OwnedGroup<Position, Lifetime>* m_G;
    uint32_t m_PosType;
};

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    printf("=== ECSPublicDemo — Fase 3: the ECS public API ===\n");

    // 1. World + records + data components + family tags.
    World world;
    OwnedGroup<Position, Velocity> moving(&world);
    OwnedGroup<Lifetime> lifetimes(&world);
    OwnedGroup<Position, Lifetime> alive(&world);

    const int kCount = 8;
    std::vector<Entity> records;
    records.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        Entity e = world.Create();
        world.Add<Tag3D>(e); // family tag (object family, no RTTI)
        world.Add<Position>(e) = { 0.0f, 0.0f, (float)i };
        if (i % 2 == 0)
            world.Add<Velocity>(e) = { 1.0f, 0.0f, 0.0f }; // even records move
        world.Add<Lifetime>(e) = { 0.4f + 0.3f * i };      // record 0 dies frame 4
        records.push_back(e);
    }
    printf("[World] created %d records (Tag3D) + data components\n", kCount);

    // A hybrid facet on one record (OOP component boxed in the ECS).
    Entity facet = world.Create();
    world.Add<Tag3D>(facet);
    world.Add<Lifetime>(facet) = { 99.0f };
    world.AddHybrid<Pinger>(facet);
    printf("[World] hybrid facet (Pinger) added on record #%u\n", facet.index);

    // Journal-synced queries: consume the structural-change journal ONCE.
    moving.Sync(world);
    lifetimes.Sync(world);
    alive.Sync(world);
    world.ClearJournal();

    // 2. Pipeline: two Update systems (one writes Position, one reads Lifetime —
    // no conflict → the scheduler runs them in parallel). The Render summary is
    // driven separately AFTER re-syncing the queries (the engine's pattern: a
    // render pass must read journal-fresh groups, never a stale one).
    CommandBuffer cb;
    SystemPipeline pipeline;
    MovementSystem moveSys(&world, &moving);
    ExpireSystem expireSys(&world, &lifetimes, &cb);
    pipeline.Add(&moveSys, SystemPhase::Update);
    pipeline.Add(&expireSys, SystemPhase::Update);

    JobSystem jobs;
    printf("[Scheduler] %u workers; pipeline has %zu systems\n", jobs.ThreadCount(), pipeline.Count());

    // 3. Hierarchy + world transforms (ECS path, one source of truth).
    HierarchyTree tree;
    TransformSystem transforms(&world, &tree);
    Entity root = world.Create();
    transforms.SetLocal(root, LocalTransform{{ 0, 0, 0 }, Quaternion::Identity(), Vector3::One()});
    Entity kid = world.Create();
    transforms.SetLocal(kid, LocalTransform{{ 2, 0, 0 }, Quaternion::Identity(), Vector3::One()});
    transforms.SetParent(kid, root);
    transforms.Update();
    auto* rootWT = transforms.GetWorld(root);
    auto* kidWT = transforms.GetWorld(kid);
    printf("[Hierarchy] root world=(%.1f,%.1f,%.1f)  kid world=(%.1f,%.1f,%.1f)\n",
           rootWT->worldPosition.x, rootWT->worldPosition.y, rootWT->worldPosition.z,
           kidWT->worldPosition.x, kidWT->worldPosition.y, kidWT->worldPosition.z);

    // 4. SoA column storage (SoAPool): one contiguous float column per field,
    // SIMD-ready — independent store for field-bulk systems.
    SoAPool<Position> colPool;
    for (int i = 0; i < 8; ++i)
        colPool.Add((uint32_t)i, Position{ (float)i, 0, 0 });
    const float before = colPool.Col(0)[3];
    std::vector<float> inc(8, 1.0f);
    Mathf::SimdAddFloats(colPool.Col(0), colPool.Col(0), inc.data(), 8);
    Position p3{};
    colPool.Get(3, p3);
    Position p4{};
    colPool.Get(4, p4);
    printf("[SoAPool] row3.x %g -> %g (SIMD column add); row4.x=%g after count=%zu\n",
           before, p3.x, p4.x, colPool.Count());

    // 5. Frame loop: Update systems run in parallel, the CommandBuffer replays
    // at the sync point (deferred destroys applied), then the queries re-sync
    // (journal consumed) and the hybrid lifecycle ticks. The Render summary
    // reads a journal-fresh query — never a stale one.
    const float dt = 0.2f;
    RenderSummarySystem renderSys(&world, &alive);
    for (int frame = 0; frame < 12; ++frame) {
        printf("--- frame %d ---\n", frame);
        pipeline.Run(0.0f, dt, &jobs, &cb, &world); // Update (parallel) + cb sync point
        moving.Sync(world);
        lifetimes.Sync(world);
        alive.Sync(world);
        world.ClearJournal();
        for (Component* c : world.GetHybrids())
            c->Tick(dt); // hybrid (OOP) lifecycle
        renderSys.Update(dt); // render pass reads fresh groups
    }

    // 6. Teardown: destroying the facet record runs the boxed OnDestroy.
    printf("--- teardown ---\n");
    world.Destroy(facet);
    printf("[World] destroying all records\n");
    for (Entity e : records)
        world.Destroy(e);
    world.Destroy(root);
    world.Destroy(kid);

    printf("=== ECSPublicDemo OK ===\n");
    return 0;
}