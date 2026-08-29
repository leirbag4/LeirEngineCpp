#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/OwnedGroup.h"
#include "LeirEngine/ECS/HierarchyTree.h"
#include "LeirEngine/ECS/TransformSystem.h"
#include "LeirEngine/ECS/System.h"
#include "LeirEngine/ECS/CommandBuffer.h"
#include "LeirEngine/ECS/HybridComponent.h"
#include "LeirEngine/ECS/Tags.h"
#include "LeirEngine/Scene/Scene.h"
#include "LeirEngine/Physics/PhysicsWorld.h"
#include "LeirEngine/Core/JobSystem.h"
#include "LeirEngine/Math/SoA.h"
#include "LeirEngine/Rendering/Frustum.h"
#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Objects/Object3D.h"
#include "LeirEngine/Components/MeshRenderer.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace Leir::ECS;
using namespace Leir;

static int g_Fails = 0;
static void Check(bool cond, const char* name)
{
    printf("%s %s\n", cond ? "ok  " : "FAIL", name);
    if (!cond) ++g_Fails;
}

struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float x = 0, y = 0, z = 0; };
struct Health { float hp = 100; };

static int g_TestCompAlive = 0;
static int g_TestCompStarts = 0;
static int g_TestCompUpdates = 0;
class TestComp : public Component {
public:
    explicit TestComp(int v = 0) : value(v) { ++g_TestCompAlive; }
    ~TestComp() { --g_TestCompAlive; }
    void OnDestroy() override { destroyed = true; }
    void OnStart() override { ++g_TestCompStarts; }
    void OnUpdate(float) override { ++g_TestCompUpdates; }
    int value = 0;
    bool destroyed = false;
};

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    World world;

    // --- Entities: creation + liveness ---
    Entity a = world.Create();
    Entity b = world.Create();
    Check(a && b && a != b, "create two distinct entities");
    Check(world.IsAlive(a) && world.IsAlive(b), "entities alive");

    // --- Component add/get/has (sparse-set pools) ---
    world.Add<Position>(a).x = 1.0f;
    world.Add<Position>(a).y = 2.0f;
    world.Add<Velocity>(a).x = 0.5f;
    Check(world.Has<Position>(a) && world.Has<Velocity>(a), "has components");
    Check(world.Has<Position>(b) == false, "other entity lacks components");
    auto* pos = world.Get<Position>(a);
    Check(pos && pos->x == 1.0f && pos->y == 2.0f, "get component value");

    // One component per type: Add returns the same instance.
    Position* p1 = world.Get<Position>(a);
    Position& p2 = world.Add<Position>(a);
    Check(p1 == &p2, "Add same type returns existing (one-per-type)");

    // --- Single-type iteration ---
    Entity c = world.Create();
    world.Add<Position>(c).z = 3.0f;
    int posCount = 0;
    float sumZ = 0.0f;
    world.Each<Position>([&](Position& p, Entity e) {
        ++posCount;
        Check(world.IsAlive(e), "iterated entity alive");
        sumZ += p.z;
    });
    Check(posCount == 2, "Each<Position> iterates 2 entities");
    Check(sumZ == 3.0f, "Each<Position> correct data");

    // --- Multi-type join (iterate entities owning BOTH) ---
    world.Add<Health>(a).hp = 50.0f;
    world.Add<Health>(b).hp = 25.0f; // b has Health + Position? no — b has no Position yet
    world.Add<Position>(b);          // now b has Position + Health
    int joined = 0;
    world.Each<Position, Health>([&](Position& p, Health& h, Entity e) {
        ++joined;
        Check(p.x >= 0.0f && h.hp > 0.0f, "join sees valid components");
        (void)e;
    });
    Check(joined == 2, "Each<Position,Health> joins a and b");

    // --- Journal + change version ---
    Check(world.GetChangeVersion() > 0, "change version bumped");
    bool sawAdded = false, sawCreated = false;
    for (const auto& rec : world.GetJournal()) {
        if (rec.type == ChangeRecord::ComponentAdded) sawAdded = true;
        if (rec.type == ChangeRecord::EntityCreated) sawCreated = true;
    }
    Check(sawAdded && sawCreated, "journal has create + component-added records");
    world.ClearJournal();
    Check(world.GetJournal().empty(), "journal cleared at sync point");

    // --- Remove component ---
    world.Remove<Health>(a);
    Check(!world.Has<Health>(a), "removed component gone");
    int afterRemove = 0;
    world.Each<Position, Health>([&](Position&, Health&, Entity e) {
        ++afterRemove;
        Check(e != a, "removed entity excluded from join");
    });
    Check(afterRemove == 1, "join keeps only entities that still own both types");

    // --- Destroy clears components; recycling bumps generation ---
    uint32_t reusedIndex = b.index;
    world.Destroy(b);
    Check(!world.IsAlive(b), "destroyed entity dead");
    Check(!world.Has<Position>(b), "destroyed entity has no components");

    Entity b2 = world.Create();
    Check(b2.index == reusedIndex, "index recycled");
    Check(b2.generation != b.generation, "generation bumped on recycle");
    Check(!world.IsAlive(b), "stale handle (old generation) stays dead");
    Check(!world.Has<Position>(b2), "recycled entity sees no stale component data");
    Check(world.Get<Position>(b2) == nullptr, "recycled entity Get is null");

    // --- Null entity guards ---
    Check(world.Get<Position>(kNullEntity) == nullptr, "null entity Get is null");
    world.Remove<Position>(kNullEntity); // no-crash
    world.Destroy(kNullEntity);          // no-crash
    Check(world.Create().index != kNullIndex, "entity 0 reserved (null)");

    // --- OwnedGroup (journal-driven cached query) ---
    World world2;
    OwnedGroup<Position, Velocity> group(&world2);

    Entity e1 = world2.Create();
    world2.Add<Position>(e1); world2.Add<Velocity>(e1); // member
    Entity e2 = world2.Create();
    world2.Add<Position>(e2);                            // NOT member (no Velocity)
    Entity e3 = world2.Create();
    world2.Add<Position>(e3); world2.Add<Velocity>(e3); // member
    Entity e4 = world2.Create();                        // empty

    group.Sync(world2);
    world2.ClearJournal();
    Check(group.Count() == 2, "group has the 2 members (not the partial/empty)");

    // Reads live data: write through the pool AFTER the group was synced.
    world2.Get<Position>(e1)->x = 9.0f;
    int seen = 0;
    group.ForEach([&](Position& p, Velocity& v, Entity e) {
        ++seen;
        if (e == e1) Check(p.x == 9.0f, "group ForEach reads live pool data");
    });
    Check(seen == 2, "group ForEach iterates members");

    // Make e2 a member by adding Velocity -> group grows after Sync.
    world2.Add<Velocity>(e2);
    group.Sync(world2);
    world2.ClearJournal();
    Check(group.Count() == 3, "group grows when a partial entity becomes a member");

    // Remove a component from a member -> it leaves the group.
    world2.Remove<Velocity>(e3);
    group.Sync(world2);
    world2.ClearJournal();
    Check(group.Count() == 2, "group shrinks when a member loses a type");

    // Destroy a member -> it leaves the group (pools cleared before the record).
    world2.Destroy(e1);
    group.Sync(world2);
    world2.ClearJournal();
    Check(group.Count() == 1, "group drops destroyed member");

    // Stable order: removing a member keeps the remaining order unchanged.
    World wS;
    OwnedGroup<Position, Velocity> sg(&wS);
    Entity sa = wS.Create();
    wS.Add<Position>(sa); wS.Add<Velocity>(sa);
    Entity sb = wS.Create();
    wS.Add<Position>(sb); wS.Add<Velocity>(sb);
    Entity sc = wS.Create();
    wS.Add<Position>(sc); wS.Add<Velocity>(sc);
    sg.Sync(wS);
    wS.ClearJournal();
    Check(sg.Count() == 3, "stable-order group has 3 members");
    wS.Remove<Velocity>(sb); // remove the middle member
    sg.Sync(wS);
    wS.ClearJournal();
    Check(sg.Count() == 2, "stable-order group shrinks");
    std::vector<uint32_t> order;
    sg.ForEach([&](Position&, Velocity&, Entity e) { order.push_back(e.index); });
    Check(order.size() == 2 && order[0] == sa.index && order[1] == sc.index,
          "stable order preserved after removal (A,C, not C,A)");
    wS.Add<Velocity>(sb); // re-add: reuses its tombstoned row -> back in original slot
    sg.Sync(wS);
    wS.ClearJournal();
    order.clear();
    sg.ForEach([&](Position&, Velocity&, Entity e) { order.push_back(e.index); });
    Check(order.size() == 3 && order[0] == sa.index && order[1] == sb.index && order[2] == sc.index,
          "stable order after re-add reuses the original slot (A,B,C)");

    // --- HierarchyTree (structure-only scene graph) ---
    World w3;
    HierarchyTree tree;
    Entity r = w3.Create();
    Entity c1 = w3.Create();
    Entity c2 = w3.Create();
    Entity gc = w3.Create();
    tree.EnsureIndex(gc.index);
    tree.SetParent(c1.index, r.index);
    tree.SetParent(c2.index, r.index);
    tree.SetParent(gc.index, c1.index);
    Check(tree.GetParent(c1.index) == r.index, "tree parent set");
    Check(tree.GetFirstChild(r.index) == c1.index, "tree first child");
    Check(tree.GetNextSibling(c1.index) == c2.index, "tree sibling order");
    Check(tree.GetLastChild(r.index) == c2.index, "tree last child");
    Check(tree.GetDepth(gc.index) == 2, "tree depth");
    Check(tree.IsDescendantOf(gc.index, r.index), "tree descendant");
    tree.SetParent(c1.index, c2.index); // move c1 under c2 (detach+reattach)
    Check(tree.GetParent(c1.index) == c2.index && tree.GetDepth(c1.index) == 2, "tree reparent updates links+depth");
    Check(tree.GetFirstChild(r.index) == c2.index, "tree detach keeps sibling links");
    tree.SetParent(r.index, c1.index); // cycle guard
    Check(tree.GetParent(r.index) == kNullIndex, "tree rejects cycle");

    // --- TransformSystem (world transform + lossy-preserve) ---
    World w4;
    HierarchyTree t4;
    TransformSystem ts(&w4, &t4);

    Entity p = w4.Create();
    Entity k = w4.Create();
    ts.SetLocal(p, LocalTransform{{1.0f, 2.0f, 3.0f}, Quaternion::Identity(), Vector3::One()});
    ts.SetLocal(k, LocalTransform{{0.0f, 0.0f, 0.0f}, Quaternion::Identity(), Vector3::One()});
    ts.SetParent(k, p, false); // keep local (worldPositionStays=false) -> inherits parent pos
    ts.Update();
    auto* kw = ts.GetWorld(k);
    Check(kw && std::fabs(kw->worldPosition.x - 1.0f) < 1e-4f
           && std::fabs(kw->worldPosition.y - 2.0f) < 1e-4f
           && std::fabs(kw->worldPosition.z - 3.0f) < 1e-4f, "child inherits parent world position");

    // The exact lossy-preserve case: parent rotated 45° + scale (2,1,1), child
    // at world identity -> after SetParent(worldPositionStays) the child's world
    // must stay identity (pos 0, rot identity, lossy 1,1,1) — no deformation.
    World w5;
    HierarchyTree t5;
    TransformSystem ts5(&w5, &t5);
    Entity pr = w5.Create();
    Entity kid = w5.Create();
    ts5.SetLocal(pr, LocalTransform{{0,0,0}, Quaternion::AngleAxis(45.0f, Vector3::Forward()), {2,1,1}});
    ts5.SetLocal(kid, LocalTransform{{0,0,0}, Quaternion::Identity(), Vector3::One()});
    ts5.SetParent(kid, pr, true);
    ts5.Update();
    auto* kk = ts5.GetWorld(kid);
    Check(kk, "child has world transform");
    Check(std::fabs(kk->worldPosition.x) < 1e-4f && std::fabs(kk->worldPosition.y) < 1e-4f
           && std::fabs(kk->worldPosition.z) < 1e-4f, "lossy-preserve keeps world position");
    Check(std::fabs(Quaternion::Dot(kk->worldRotation, Quaternion::Identity()) - 1.0f) < 1e-4f,
          "lossy-preserve keeps world rotation (no deformation)");
    Check(std::fabs(kk->worldScale.x - 1.0f) < 1e-3f
           && std::fabs(kk->worldScale.y - 1.0f) < 1e-3f
           && std::fabs(kk->worldScale.z - 1.0f) < 1e-3f, "lossy-preserve keeps world lossy scale");

    // Moving the parent moves the child; dirty frontier recomputes on demand.
    ts5.SetLocal(pr, LocalTransform{{5.0f, 0.0f, 0.0f}, Quaternion::Identity(), Vector3::One()});
    auto* moved = ts5.GetWorld(kid); // ensures clean
    Check(moved && std::fabs(moved->worldPosition.x - 5.0f) < 1e-4f, "parent move propagates to child");

    // Zero-scaled parent axis (epsilon guard): must stay finite, no NaN.
    World w6;
    HierarchyTree t6;
    TransformSystem ts6(&w6, &t6);
    Entity zp = w6.Create();
    ts6.SetLocal(zp, LocalTransform{{0,0,0}, Quaternion::Identity(), {0,1,1}});
    Entity z = w6.Create();
    ts6.SetParent(z, zp, true);
    ts6.Update();
    auto* zw = ts6.GetWorld(z);
    Check(zw && std::isfinite(zw->worldScale.x) && std::isfinite(zw->worldScale.y)
           && std::isfinite(zw->worldPosition.x), "zero-scaled axis stays finite");

    // Regression: many siblings force the WorldTransform pool to grow; the
    // parent's world must be read BEFORE the child's Add reallocates the pool
    // (dangling-pointer UB surfaced as NaN on AppleClang).
    World w9;
    HierarchyTree t9;
    TransformSystem ts9(&w9, &t9);
    Entity root9 = w9.Create();
    ts9.SetLocal(root9, LocalTransform{{0,0,0}, Quaternion::Identity(), Vector3::One()});
    bool allGood = true;
    for (int i = 0; i < 200; ++i) {
        Entity c = w9.Create();
        ts9.SetLocal(c, LocalTransform{{(float)i, 0, 0}, Quaternion::Identity(), Vector3::One()});
        ts9.SetParent(c, root9, true);
        ts9.Update();
        auto* wc = ts9.GetWorld(c);
        if (!wc || !std::isfinite(wc->worldPosition.x) || std::fabs(wc->worldPosition.x - (float)i) > 1e-3f)
            allGood = false;
    }
    Check(allGood, "many-children pool growth stays finite/correct");

    // --- Systems pipeline + command buffer (deferred structural changes) ---
    class MoveSystem : public ISystem {
    public:
        MoveSystem(World* world, OwnedGroup<Position, Velocity>* g)
            : ISystem("Move"), m_G(g),
              m_PosType(world->ComponentType<Position>()),
              m_VelType(world->ComponentType<Velocity>()) {}
        std::vector<SystemAccess> GetAccess() const override { return {{m_PosType, true}, {m_VelType, false}}; }
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

    class ExpireSystem : public ISystem {
    public:
        ExpireSystem(World* world, OwnedGroup<Health>* g, CommandBuffer* cb)
            : ISystem("Expire"), m_G(g), m_CB(cb), m_HealthType(world->ComponentType<Health>()) {}
        std::vector<SystemAccess> GetAccess() const override { return {{m_HealthType, false}}; }
        void Update(float) override
        {
            m_G->ForEach([this](Health& h, Entity e) {
                if (h.hp <= 0.0f) m_CB->Destroy(e);
            });
        }
    private:
        OwnedGroup<Health>* m_G;
        CommandBuffer* m_CB;
        uint32_t m_HealthType;
    };

    class ReadPosSystem : public ISystem {
    public:
        ReadPosSystem(World* world, OwnedGroup<Position>* g)
            : ISystem("ReadPos"), m_G(g), m_PosType(world->ComponentType<Position>()) {}
        std::vector<SystemAccess> GetAccess() const override { return {{m_PosType, false}}; }
        void Update(float) override
        {
            m_G->ForEach([this](Position& p, Entity) { m_Sum += p.x; });
        }
        float m_Sum = 0.0f;
    private:
        OwnedGroup<Position>* m_G;
        uint32_t m_PosType;
    };

    World w7;
    OwnedGroup<Position, Velocity> moveGroup(&w7);
    OwnedGroup<Health> healthGroup(&w7);
    CommandBuffer cb7;
    MoveSystem moveSys(&w7, &moveGroup);
    ExpireSystem expireSys(&w7, &healthGroup, &cb7);
    SystemPipeline pipeline;
    pipeline.Add(&moveSys, SystemPhase::Update);
    pipeline.Add(&expireSys, SystemPhase::Update);

    Entity m1 = w7.Create();
    w7.Add<Position>(m1).x = 0.0f; w7.Add<Velocity>(m1).x = 1.0f;
    Entity m2 = w7.Create();
    w7.Add<Position>(m2).x = 0.0f; w7.Add<Velocity>(m2).x = 2.0f;
    Entity dead = w7.Create();
    w7.Add<Health>(dead).hp = 0.0f; // expires -> deferred destroy
    moveGroup.Sync(w7);
    healthGroup.Sync(w7);
    w7.ClearJournal();

    pipeline.Run(0.0f, 1.0f); // fixedDt unused here
    Check(w7.Get<Position>(m1)->x == 1.0f && w7.Get<Position>(m2)->x == 2.0f,
          "MoveSystem ran in Update phase (velocity * dt)");
    Check(!cb7.IsEmpty(), "ExpireSystem enqueued a deferred destroy");
    cb7.Replay(w7); // sync point
    Check(!w7.IsAlive(dead), "command buffer destroy applied at replay");
    Check(w7.IsAlive(m1) && w7.IsAlive(m2), "replay did not touch living entities");

    // Deferred add with data.
    CommandBuffer cb8;
    World w8;
    Entity n = w8.Create();
    cb8.Add<Position>(n, Position{7.0f, 8.0f, 9.0f});
    cb8.Replay(w8);
    auto* np = w8.Get<Position>(n);
    Check(np && np->x == 7.0f && np->y == 8.0f && np->z == 9.0f, "deferred add carries data");
    cb8.Remove<Position>(n);
    cb8.Replay(w8);
    Check(!w8.Has<Position>(n), "deferred remove applied");

    // --- Fase 2: parallel systems scheduler (dependency-ordered) ---
    {
        World wp;
        OwnedGroup<Position, Velocity> pg(&wp);
        OwnedGroup<Position> posOnly(&wp);
        MoveSystem moveA(&wp, &pg);
        ReadPosSystem readA(&wp, &posOnly);
        SystemPipeline pp;
        pp.Add(&moveA, SystemPhase::Update); // writes Position
        pp.Add(&readA, SystemPhase::Update); // reads Position -> conflicts -> after Move

        // Sequential reference run.
        Entity pa = wp.Create();
        wp.Add<Position>(pa).x = 0.0f;
        wp.Add<Velocity>(pa).x = 3.0f;
        pg.Sync(wp);
        posOnly.Sync(wp);
        wp.ClearJournal();
        pp.Run(0.0f, 1.0f);
        Check(readA.m_Sum == 3.0f, "sequential: ReadPos saw the moved position (dependency order)");

        // Parallel run must produce the SAME result (scheduler enforces order).
        Entity pb = wp.Create();
        wp.Add<Position>(pb).x = 10.0f;
        wp.Add<Velocity>(pb).x = 1.0f;
        pg.Sync(wp);
        posOnly.Sync(wp);
        wp.ClearJournal();
        JobSystem jobs;
        readA.m_Sum = 0.0f;
        pp.Run(0.0f, 1.0f, &jobs);
        // Both entities moved again (pa 3->6, pb 10->11); ReadPos ran after Move.
        Check(readA.m_Sum == 17.0f, "parallel: scheduler kept dependency order (6+11)");
    }

    // Command buffer replayed at the sync points during a parallel run.
    {
        World wc;
        OwnedGroup<Health> hg(&wc);
        CommandBuffer cb;
        ExpireSystem exp(&wc, &hg, &cb);
        SystemPipeline pp;
        pp.Add(&exp, SystemPhase::Update);
        Entity e1 = wc.Create();
        wc.Add<Health>(e1).hp = 0.0f;
        Entity e2 = wc.Create();
        wc.Add<Health>(e2).hp = 50.0f;
        hg.Sync(wc);
        wc.ClearJournal();
        JobSystem jobs;
        pp.Run(0.0f, 1.0f, &jobs, &cb, &wc); // Update phase enqueues -> replayed at sync point
        Check(!wc.IsAlive(e1), "command buffer replayed at the sync point (parallel-safe)");
        Check(wc.IsAlive(e2), "living entity untouched");
        Check(cb.IsEmpty(), "command buffer drained after replay");
    }

    // --- HybridComponent (OOP component boxed in the ECS) ---
    World hw;
    Entity he = hw.Create();
    auto& comp = hw.AddHybrid<TestComp>(he, 42);
    Check(comp.value == 42, "AddHybrid creates and wires the OOP instance");
    Check(g_TestCompAlive == 1, "boxed component alive");
    auto* got = hw.GetHybrid<TestComp>(he);
    Check(got && got == &comp, "GetHybrid returns the live instance");
    auto& comp2 = hw.AddHybrid<TestComp>(he, 7);
    Check(&comp2 == &comp && comp2.value == 42, "AddHybrid one-per-type returns existing");

    OwnedGroup<HybridComponent<TestComp>> hg(&hw);
    hg.Sync(hw);
    hw.ClearJournal();
    int iter = 0;
    hg.ForEach([&](HybridComponent<TestComp>& hc, Entity) {
        ++iter;
        Check(hc.instance && hc.instance->value == 42, "group iterates boxed components");
    });
    Check(iter == 1, "group over HybridComponent works");

    hw.Destroy(he);
    Check(g_TestCompAlive == 0, "destroying entity destroys boxed component");

    // --- Scene (ECS-backed ISceneStorage; Etapa A fusion) ---
    Scene es;
    Object3D* root = es.CreateObject3D("root");
    root->AddComponent<MeshRenderer>();
    Object3D* child = es.CreateObject3D("child");
    root->AddChild(child);
    Object3D* leaf = es.CreateObject3D("leaf");
    child->AddChild(leaf);

    es.OnUpdate(0.0f);

    // Family tags on the entities.
    Entity rootE = es.EntityOf(root);
    Entity leafE = es.EntityOf(leaf);
    Check(es.GetWorld().Has<Tag3D>(rootE), "root entity tagged 3D");
    Check(es.GetTree().GetParent(leafE.index) == es.EntityOf(child).index, "ecs tree mirrors hierarchy");

    // ECS world transform == the OOP reference world.
    auto* rootWT = es.GetTransforms().GetWorld(rootE);
    auto* leafWT = es.GetTransforms().GetWorld(leafE);
    Check(rootWT && leafWT, "ecs computed world transforms");
    bool worldMatches = false;
    if (rootWT && leafWT) {
        worldMatches = true;
        Matrix4x4 ref = leaf->GetTransform().GetLocalToWorldMatrix();
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (std::fabs((*leafWT).worldMatrix(r, c) - ref(r, c)) > 1e-3f) worldMatches = false;
    }
    Check(worldMatches, "ecs world matches the OOP reference");

    // Lossy-preserve reparent through the friendly API, mirrored to the ECS.
    Object3D* rotated = es.CreateObject3D("rotated");
    rotated->GetTransform().SetLocalRotation(Quaternion::AngleAxis(45.0f, Vector3::Forward()));
    rotated->GetTransform().SetLocalScale({2, 1, 1});
    leaf->SetParent(rotated, true); // worldPositionStays
    es.OnUpdate(0.0f);
    auto* leafWT2 = es.GetTransforms().GetWorld(leafE);
    Check(leafWT2 && std::fabs(leafWT2->worldScale.x - 1.0f) < 1e-3f
           && std::fabs(leafWT2->worldScale.y - 1.0f) < 1e-3f
           && std::fabs(leafWT2->worldScale.z - 1.0f) < 1e-3f, "ecs lossy-preserve reparent (no deformation)");

    // ISceneStorage contract: renderables (MeshRenderer on root), objects list.
    bool hasRoot = false;
    for (auto* o : es.GetRenderables()) if (o == root) hasRoot = true;
    Check(hasRoot, "scene renderables include the MeshRenderer object");
    Check(es.GetObjects().size() == 4, "scene GetObjects lists all objects");
    Check(es.FindObjectByUUID(root->GetUUID()) == root, "scene FindObjectByUUID");

    // AddChild links the transform (Unity semantics): a child created at world
    // (5,5,5) under a parent at (1,2,3) STAYS at (5,5,5) and its local becomes
    // (4,3,2). Before the fix AddChild only wired the tree -> the child drifted.
    Scene es2;
    Object3D* pa = es2.CreateObject3D("pa");
    pa->GetTransform().SetLocalPosition(Leir::Vector3(1, 2, 3));
    Object3D* ca = es2.CreateObject3D("ca");
    ca->GetTransform().SetLocalPosition(Leir::Vector3(5, 5, 5));
    pa->AddChild(ca);
    es2.OnUpdate(0.0f);
    Vector3 wca = ca->GetTransform().GetWorldPosition();
    Vector3 lca = ca->GetTransform().GetLocalPosition();
    Check(std::fabs(wca.x - 5) < 1e-3f && std::fabs(wca.y - 5) < 1e-3f && std::fabs(wca.z - 5) < 1e-3f,
          "AddChild keeps child world");
    Check(std::fabs(lca.x - 4) < 1e-3f && std::fabs(lca.y - 3) < 1e-3f && std::fabs(lca.z - 2) < 1e-3f,
          "AddChild re-derives child local");
    // Second frame must NOT drift (the old bug: local<-world each frame).
    es2.OnUpdate(0.0f);
    Vector3 wca2 = ca->GetTransform().GetWorldPosition();
    Check(std::fabs(wca2.x - 5) < 1e-3f && std::fabs(wca2.y - 5) < 1e-3f && std::fabs(wca2.z - 5) < 1e-3f,
          "AddChild child world stable across frames (no drift)");

    // --- Transform ECS-backing facade (Etapa A, increment A1) ---
    World wA;
    HierarchyTree tA;
    TransformSystem tsA(&wA, &tA);
    Entity rootA = wA.Create();
    Transform rootT;
    rootT.SetEcsBacked(&wA, &tsA, &tA, rootA);
    rootT.SetLocalPosition({1, 2, 3}); // mirrors into the ECS LocalTransform
    rootT.SetLocalRotation(Quaternion::AngleAxis(45.0f, Vector3::Forward()));
    rootT.SetLocalScale({2, 1, 1});
    Vector3 rp = rootT.GetWorldPosition(); // reads the ECS WorldTransform
    Check(std::fabs(rp.x - 1) < 1e-4f && std::fabs(rp.y - 2) < 1e-4f && std::fabs(rp.z - 3) < 1e-4f,
          "backed root world position from ECS");

    Entity childA = wA.Create();
    Transform childT;
    childT.SetEcsBacked(&wA, &tsA, &tA, childA);
    childT.SetParent(&rootT, true); // ECS tree + worldPositionStays (lossy-preserve)
    Vector3 cs = childT.GetWorldScale();
    Check(std::fabs(cs.x - 1) < 1e-3f && std::fabs(cs.y - 1) < 1e-3f && std::fabs(cs.z - 1) < 1e-3f,
          "backed child world scale preserved (lossy, no deformation)");
    Check(std::fabs(Quaternion::Dot(childT.GetWorldRotation(), Quaternion::Identity()) - 1.0f) < 1e-3f,
          "backed child world rotation identity");

    // SetWorldScale on the backed child recomputes the local via the ECS.
    childT.SetWorldScale({1, 1, 1});
    Vector3 cl = childT.GetLocalScale();
    Check(std::fabs(cl.x - 0.632f) < 1e-2f && std::fabs(cl.y - 0.632f) < 1e-2f && std::fabs(cl.z - 1.0f) < 1e-2f,
          "backed SetWorldScale lossy-preserve local");

    // --- CoreObject ECS-backed components (Etapa A, increment A2) ---
    World wA2;
    HierarchyTree tA2;
    TransformSystem tsA2(&wA2, &tA2);
    Object3D* bo = new Object3D("Backed");
    Entity be = wA2.Create();
    tA2.EnsureIndex(be.index);
    bo->GetTransform().SetEcsBacked(&wA2, &tsA2, &tA2, be);
    auto& bc = bo->AddComponent<TestComp>(99);
    Check(bc.value == 99, "backed AddComponent boxes into the ECS hybrid");
    Check(wA2.GetHybrid<TestComp>(be) == &bc, "backed component lives in the ECS world");
    auto* bg = bo->GetComponent<TestComp>();
    Check(bg && bg == &bc, "backed GetComponent returns the ECS boxed instance");
    auto& bc2 = bo->AddComponent<TestComp>(1);
    Check(&bc2 == &bc && bc2.value == 99, "backed AddComponent one-per-type");
    bo->RemoveComponent<TestComp>();
    Check(!bo->GetComponent<TestComp>(), "backed RemoveComponent removes the hybrid");
    delete bo;

    // --- Hybrid lifecycle registry (A3 groundwork) ---
    World wL;
    HierarchyTree tL;
    TransformSystem tsL(&wL, &tL);
    Object3D* lo = new Object3D("L");
    Entity le = wL.Create();
    tL.EnsureIndex(le.index);
    lo->GetTransform().SetEcsBacked(&wL, &tsL, &tL, le);
    g_TestCompStarts = 0;
    g_TestCompUpdates = 0;
    lo->AddComponent<TestComp>(7);
    Check(wL.GetHybrids().size() == 1, "hybrid registered in the lifecycle registry");
    for (auto* c : wL.GetHybrids())
        c->Tick(0.016f);
    Check(g_TestCompStarts == 1 && g_TestCompUpdates == 1, "hybrid lifecycle start+update");
    for (auto* c : wL.GetHybrids())
        c->Tick(0.016f);
    Check(g_TestCompStarts == 1 && g_TestCompUpdates == 2, "hybrid start once, update each tick");
    lo->RemoveComponent<TestComp>();
    Check(wL.GetHybrids().empty(), "removing hybrid unregisters it from the registry");
    delete lo;

    // The fused Scene::OnUpdate steps physics (lazy-inits Jolt); shut it down so
    // macOS doesn't abort in Jolt's thread teardown at process exit.
    PhysicsWorld::GetInstance().Shutdown();

    // --- Fase 2: SIMD mat4x4 multiply matches the scalar glm result ---
    {
        uint32_t seed = 12345u;
        auto rnd = [&seed]() {
            seed = seed * 1664525u + 1013904223u;
            return (float)(seed >> 8) / 16777216.0f * 20.0f - 10.0f;
        };
        bool simdOk = true;
        float maxDiff = 0.0f;
        for (int t = 0; t < 200; ++t) {
            Matrix4x4 a, b;
            for (float& v : a.m) v = rnd();
            for (float& v : b.m) v = rnd();
            Matrix4x4 c1 = a * b; // glm scalar
            Matrix4x4 c2 = Matrix4x4::MultiplySimd(a, b);
            for (int i = 0; i < 16; ++i) {
                float d = std::fabs(c1.m[i] - c2.m[i]);
                if (d > maxDiff) maxDiff = d;
            }
        }
        // FMA accumulates with a single rounding per term vs glm's two — the
        // results agree to float precision, not bit-exactly.
        Check(maxDiff < 5e-3f, "SIMD mat4x4 multiply matches scalar (float precision)");
    }

    // --- Fase 2: JobSystem (thread pool + ParallelFor + Dispatch/WaitAll) ---
    {
        JobSystem js;
        const size_t n = 10000;
        std::atomic<long long> sum{0};
        js.ParallelFor(n, [&sum](size_t i) { sum += (long long)i; });
        Check(sum == (long long)n * (long long)(n - 1) / 2, "JobSystem ParallelFor sums correctly");

        std::atomic<int> tasks{0};
        for (int i = 0; i < 50; ++i)
            js.Dispatch([&tasks]() { tasks.fetch_add(1); });
        js.WaitAll();
        Check(tasks == 50, "JobSystem Dispatch/WaitAll runs every task");
    }

    // --- Fase 2: benchmarks (informational; not asserted, CI-stable) ---
    {
        using Clock = std::chrono::steady_clock;
        auto ms = [](Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        };

        // SIMD vs scalar mat4x4 multiply.
        {
            Matrix4x4 a, b, c;
            uint32_t seed = 99u;
            auto rnd = [&seed]() {
                seed = seed * 1664525u + 1013904223u;
                return (float)(seed >> 8) / 16777216.0f * 2.0f - 1.0f;
            };
            for (float& v : a.m) v = rnd();
            for (float& v : b.m) v = rnd();
            const int iters = 2000000;
            auto t0 = Clock::now();
            for (int i = 0; i < iters; ++i) c = a * b;
            auto t1 = Clock::now();
            for (int i = 0; i < iters; ++i) c = Matrix4x4::MultiplySimd(a, b);
            auto t2 = Clock::now();
            double scalarMs = ms(t0, t1), simdMs = ms(t1, t2);
            printf("bench: mat4x4 multiply scalar=%.2f ms simd=%.2f ms (x%.2f)\n",
                   scalarMs, simdMs, scalarMs / simdMs);
        }

        // JobSystem ParallelFor vs sequential — CPU-bound (mat4x4 per index):
        // distinct-index writes (no races), work per item >> scheduling cost.
        {
            const size_t n = 200000;
            Matrix4x4 a, b;
            uint32_t seed = 7u;
            auto rnd = [&seed]() {
                seed = seed * 1664525u + 1013904223u;
                return (float)(seed >> 8) / 16777216.0f * 2.0f - 1.0f;
            };
            for (float& v : a.m) v = rnd();
            for (float& v : b.m) v = rnd();
            std::vector<float> sink(n, 0.0f);
            auto t0 = Clock::now();
            for (size_t i = 0; i < n; ++i) { Matrix4x4 r = a * b; sink[i] = r.m[0]; }
            auto t1 = Clock::now();
            JobSystem jobs;
            auto t2 = Clock::now();
            jobs.ParallelFor(n, [&](size_t i) { Matrix4x4 r = a * b; sink[i] = r.m[0]; });
            auto t3 = Clock::now();
            double seqMs = ms(t0, t1), parMs = ms(t2, t3);
            printf("bench: ParallelFor(mat4x4) seq=%.2f ms par=%.2f ms (%u threads, x%.2f)\n",
                   seqMs, parMs, jobs.ThreadCount(), seqMs / parMs);
        }

        // SoA column SIMD vs scalar (Incremento 4): add 2 float columns, 4-wide.
        {
            const size_t n = 4000000;
            std::vector<float> a(n), b(n), simdOut(n), scalarOut(n);
            uint32_t seed = 12u;
            auto rnd = [&seed]() {
                seed = seed * 1664525u + 1013904223u;
                return (float)(seed >> 8) / 16777216.0f * 2.0f - 1.0f;
            };
            for (size_t i = 0; i < n; ++i) { a[i] = rnd(); b[i] = rnd(); }
            auto t0 = Clock::now();
            for (size_t i = 0; i < n; ++i) scalarOut[i] = a[i] + b[i];
            auto t1 = Clock::now();
            Mathf::SimdAddFloats(simdOut.data(), a.data(), b.data(), n);
            auto t2 = Clock::now();
            double scalarMs = ms(t0, t1), simdMs = ms(t1, t2);
            bool ok = true;
            for (size_t i = 0; i < n; ++i)
                if (simdOut[i] != scalarOut[i]) { ok = false; break; }
            Check(ok, "SoA SimdAddFloats matches scalar exactly");
            printf("bench: SoA add(scalar) %.2f ms simd=%.2f ms (x%.2f)\n",
                   scalarMs, simdMs, scalarMs / simdMs);
        }

        // Render list vectorizado (Fase 2): SIMD frustum cull vs scalar over
        // 100k synthetic renderables (sphere centers + radii).
        {
            Matrix4x4 viewProj = Matrix4x4::Perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f) *
                                 Matrix4x4::LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
            Frustum fr;
            fr.Extract(viewProj);

            const size_t n = 100000;
            std::vector<Vector3> centers(n);
            std::vector<float> radii(n);
            uint32_t seed = 21u;
            auto rnd = [&seed](float lo, float hi) {
                seed = seed * 1664525u + 1013904223u;
                return lo + (float)(seed >> 8) / 16777216.0f * (hi - lo);
            };
            for (size_t i = 0; i < n; ++i) {
                centers[i] = { rnd(-60.0f, 60.0f), rnd(-60.0f, 60.0f), rnd(-60.0f, 60.0f) };
                radii[i] = rnd(0.1f, 3.0f);
            }

            auto t0 = Clock::now();
            size_t simdVisible = 0;
            for (size_t i = 0; i < n; ++i)
                if (fr.TestSphere(centers[i], radii[i])) ++simdVisible;
            auto t1 = Clock::now();
            size_t scalarVisible = 0;
            for (size_t i = 0; i < n; ++i)
                if (fr.TestSphereScalar(centers[i], radii[i])) ++scalarVisible;
            auto t2 = Clock::now();
            double simdMs = ms(t0, t1), scalarMs = ms(t1, t2);
            Check(simdVisible == scalarVisible, "SIMD frustum cull matches scalar exactly");
            // Debug note: __m128 values spill to the stack per call in Debug,
            // so the per-call SIMD looks slower here (x0.55). Release /O2
            // (measured standalone, same code) is x3.55 faster — the vectorized
            // cull is what runs in the render list build.
            printf("bench: frustum cull 100k simd=%.2f ms scalar=%.2f ms (Debug spill; /O2 x3.55, %zu visible)\n",
                   simdMs, scalarMs, simdVisible);
        }
    }

    printf(g_Fails == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", g_Fails);
    return g_Fails == 0 ? 0 : 1;
}