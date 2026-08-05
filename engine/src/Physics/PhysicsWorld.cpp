#include "LeirEngine/Physics/PhysicsWorld.h"
#include "PhysicsConversions.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

#include "LeirEngine/Core/Log.h"

JPH_SUPPRESS_WARNINGS

namespace Leir {

// ---- Singleton ----

PhysicsWorld& PhysicsWorld::GetInstance() {
    static PhysicsWorld instance;
    return instance;
}

// ---- Init / Shutdown ----

void PhysicsWorld::Init() {
    if (m_Initialized) return;

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_TempAllocator = new JPH::TempAllocatorMalloc();

    uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_JobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        numThreads
    );

    constexpr uint32_t NUM_OBJECT_LAYERS = 2;
    constexpr uint32_t NUM_BROAD_PHASE_LAYERS = 2;

    auto* bpLayerInterface = new JPH::BroadPhaseLayerInterfaceTable(NUM_OBJECT_LAYERS, NUM_BROAD_PHASE_LAYERS);
    bpLayerInterface->MapObjectToBroadPhaseLayer(PhysicsLayers::NON_MOVING, JPH::BroadPhaseLayer(0));
    bpLayerInterface->MapObjectToBroadPhaseLayer(PhysicsLayers::MOVING, JPH::BroadPhaseLayer(1));

    auto* objectLayerPairFilter = new JPH::ObjectLayerPairFilterTable(NUM_OBJECT_LAYERS);
    objectLayerPairFilter->EnableCollision(PhysicsLayers::NON_MOVING, PhysicsLayers::MOVING);
    objectLayerPairFilter->EnableCollision(PhysicsLayers::MOVING, PhysicsLayers::MOVING);

    auto* objectVsBPFilter = new JPH::ObjectVsBroadPhaseLayerFilterTable(
        *bpLayerInterface, NUM_BROAD_PHASE_LAYERS, *objectLayerPairFilter, NUM_OBJECT_LAYERS
    );

    const uint32_t maxBodies = 1024;
    const uint32_t numBodyMutexes = 0;
    const uint32_t maxBodyPairs = 65536;
    const uint32_t maxContactConstraints = 65536;

    m_PhysicsSystem = new JPH::PhysicsSystem();
    m_PhysicsSystem->Init(
        maxBodies,
        numBodyMutexes,
        maxBodyPairs,
        maxContactConstraints,
        *bpLayerInterface,
        *objectVsBPFilter,
        *objectLayerPairFilter
    );

    m_PhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    m_Initialized = true;
    XConsole::Println("PhysicsWorld initialized");
}

void PhysicsWorld::Shutdown() {
    if (!m_Initialized) return;

    delete m_PhysicsSystem;
    m_PhysicsSystem = nullptr;

    delete m_JobSystem;
    m_JobSystem = nullptr;

    delete m_TempAllocator;
    m_TempAllocator = nullptr;

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_Initialized = false;
    XConsole::Println("PhysicsWorld shutdown");
}

PhysicsWorld::~PhysicsWorld() {
    if (m_Initialized) Shutdown();
}

// ---- Step ----

void PhysicsWorld::StepPhysics(float deltaTime) {
    if (!m_Initialized) Init();

    const int collisionSteps = 1;
    m_PhysicsSystem->Update(deltaTime, collisionSteps, m_TempAllocator, m_JobSystem);
}

// ---- Accessors ----

JPH::BodyInterface& PhysicsWorld::GetBodyInterface() {
    return m_PhysicsSystem->GetBodyInterface();
}

const JPH::BodyInterface& PhysicsWorld::GetBodyInterface() const {
    return m_PhysicsSystem->GetBodyInterface();
}

JPH::PhysicsSystem& PhysicsWorld::GetPhysicsSystem() {
    return *m_PhysicsSystem;
}

} // namespace Leir
