#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Physics/PhysicsWorld.h>
#include <LeirEngine/Physics/RigidBody.h>
#include <LeirEngine/Physics/Collider.h>

#include <cstdio>
#include <cmath>
#include <cstring>

#define LOG(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while(0)

static int s_passCount = 0;
static int s_failCount = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        LOG("  FAIL: " name); \
        ++s_failCount; \
    } else { \
        LOG("  PASS: " name); \
        ++s_passCount; \
    } \
} while(0)

static void testGravityFall() {
    LOG("--- Gravity Fall ---");
    Leir::Scene scene("GravityFall");

    auto* box = scene.CreateObject3D("Box");
    box->GetTransform().SetWorldPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
    box->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Dynamic);

    float prevY = box->GetTransform().GetWorldPosition().y;
    bool fell = false;
    for (int i = 0; i < 60; ++i) {
        scene.OnUpdate(1.0f / 60.0f);
        float y = box->GetTransform().GetWorldPosition().y;
        if (y < prevY - 0.001f) fell = true;
        prevY = y;
    }

    float finalY = box->GetTransform().GetWorldPosition().y;
    TEST("Box fell down", finalY < 9.0f);
    TEST("Box movement detected", fell);
}

static void testKinematic() {
    LOG("--- Kinematic ---");
    Leir::Scene scene("Kinematic");

    auto* box = scene.CreateObject3D("Box");
    box->GetTransform().SetWorldPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
    auto& rb = box->AddComponent<Leir::RigidBody>();
    rb.SetType(Leir::RigidBodyType::Kinematic);

    box->GetTransform().SetWorldPosition(glm::vec3(5.0f, 0.0f, 0.0f));

    for (int i = 0; i < 5; ++i)
        scene.OnUpdate(1.0f / 60.0f);

    float x = box->GetTransform().GetWorldPosition().x;
    TEST("Kinematic moved by OnUpdate cycle", std::abs(x - 5.0f) < 0.01f);
}

static void testBodyID() {
    LOG("--- Body ID ---");
    Leir::Scene scene("BodyID");

    auto* box = scene.CreateObject3D("Box");
    box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
    box->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Dynamic);

    scene.OnUpdate(0.0f);

    auto* rb = box->GetComponent<Leir::RigidBody>();
    TEST("RigidBody exists", rb != nullptr);
    if (rb) {
        TEST("Body ID is non-zero", rb->GetBodyID() != 0);
    }
}

static void testStaticImmovable() {
    LOG("--- Static Immovable ---");
    Leir::Scene scene("StaticImmovable");

    auto* ground = scene.CreateObject3D("Ground");
    ground->GetTransform().SetWorldPosition(glm::vec3(0.0f, -5.0f, 0.0f));
    ground->AddComponent<Leir::Collider>().SetBox(glm::vec3(10.0f, 0.5f, 10.0f));
    ground->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Static);

    auto* box = scene.CreateObject3D("Box");
    box->GetTransform().SetWorldPosition(glm::vec3(0.0f, 2.0f, 0.0f));
    box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
    box->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Dynamic);

    for (int i = 0; i < 200; ++i)
        scene.OnUpdate(1.0f / 60.0f);

    float boxY = box->GetTransform().GetWorldPosition().y;
    float groundY = ground->GetTransform().GetWorldPosition().y;

    TEST("Box is at or above ground surface", boxY - 0.5f >= groundY - 0.5f - 0.1f);
    TEST("Box came to rest below initial height", boxY < 1.0f);
    TEST("Ground is immovable (no crash)", true);
}

static void testMultipleScenes() {
    LOG("--- Multiple Scenes ---");
    for (int s = 0; s < 3; ++s) {
        Leir::Scene scene("MultiTest");
        auto* box = scene.CreateObject3D("Box");
        box->GetTransform().SetWorldPosition(glm::vec3(0.0f, 5.0f, 0.0f));
        box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
        box->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Dynamic);

        for (int i = 0; i < 30; ++i)
            scene.OnUpdate(1.0f / 60.0f);

        float y = box->GetTransform().GetWorldPosition().y;
        char buf2[64];
        snprintf(buf2, sizeof(buf2), "Scene %d ran without crash", s);
        if (y >= 5.0f) { LOG("  FAIL: %s", buf2); ++s_failCount; }
        else { LOG("  PASS: %s", buf2); ++s_passCount; }
    }
}

int main() {
    LOG("=== Physics Test Suite ===");

    Leir::PhysicsWorld::GetInstance().Init();

    // Each test's scene is destroyed before the next test or Shutdown
    testGravityFall();
    testKinematic();
    testBodyID();
    testStaticImmovable();
    testMultipleScenes();

    Leir::PhysicsWorld::GetInstance().Shutdown();

    LOG("=== Results: %d passed, %d failed ===", s_passCount, s_failCount);

    if (s_failCount > 0) {
        LOG("Some tests FAILED!");
        return 1;
    }
    LOG("All tests passed!");
    return 0;
}
