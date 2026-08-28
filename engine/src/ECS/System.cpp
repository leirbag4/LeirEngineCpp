#include "LeirEngine/ECS/System.h"
#include "LeirEngine/ECS/CommandBuffer.h"

namespace Leir {
namespace ECS {

void SystemPipeline::Add(ISystem* system, SystemPhase phase)
{
    if (!system)
        return;
    switch (phase) {
    case SystemPhase::FixedUpdate: m_Fixed.push_back(system); break;
    case SystemPhase::Update: m_Update.push_back(system); break;
    case SystemPhase::Render: m_Render.push_back(system); break;
    }
}

void SystemPipeline::Run(float fixedDt, float dt)
{
    for (ISystem* s : m_Fixed)
        s->Update(fixedDt);
    for (ISystem* s : m_Update)
        s->Update(dt);
    for (ISystem* s : m_Render)
        s->Update(dt);
}

void CommandBuffer::Replay(World& world)
{
    for (auto& op : m_Ops)
        op(world);
    m_Ops.clear();
}

} // namespace ECS
} // namespace Leir