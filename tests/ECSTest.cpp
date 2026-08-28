#include "LeirEngine/ECS/World.h"

#include <cstdio>

using namespace Leir::ECS;

static int g_Fails = 0;
static void Check(bool cond, const char* name)
{
    printf("%s %s\n", cond ? "ok  " : "FAIL", name);
    if (!cond) ++g_Fails;
}

struct Position { float x = 0, y = 0, z = 0; };
struct Velocity { float x = 0, y = 0, z = 0; };
struct Health { float hp = 100; };

int main()
{
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

    printf(g_Fails == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", g_Fails);
    return g_Fails == 0 ? 0 : 1;
}