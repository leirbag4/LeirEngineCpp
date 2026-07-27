#include "LeirEngine/Scene/SceneManager.h"

#include <algorithm>

namespace Leir {

SceneManager& SceneManager::GetInstance()
{
    static SceneManager instance;
    return instance;
}

Scene& SceneManager::CreateScene(const std::string& name)
{
    auto scene = std::make_unique<Scene>(name);
    Scene* ptr = scene.get();
    m_Scenes.push_back(std::move(scene));

    if (!m_ActiveScene)
        m_ActiveScene = ptr;

    return *ptr;
}

void SceneManager::DestroyScene(Scene* scene)
{
    if (!scene) return;

    auto it = std::find_if(m_Scenes.begin(), m_Scenes.end(),
        [scene](const std::unique_ptr<Scene>& s) { return s.get() == scene; });

    if (it != m_Scenes.end()) {
        if (m_ActiveScene == it->get())
            m_ActiveScene = nullptr;
        m_Scenes.erase(it);
    }
}

void SceneManager::DestroyScene(const std::string& name)
{
    for (auto it = m_Scenes.begin(); it != m_Scenes.end(); ++it) {
        if ((*it)->GetName() == name) {
            if (m_ActiveScene == it->get())
                m_ActiveScene = nullptr;
            m_Scenes.erase(it);
            return;
        }
    }
}

void SceneManager::SetActiveScene(Scene* scene)
{
    m_ActiveScene = scene;
}

void SceneManager::SetActiveScene(const std::string& name)
{
    m_ActiveScene = FindScene(name);
}

Scene* SceneManager::FindScene(const std::string& name) const
{
    for (const auto& scene : m_Scenes) {
        if (scene->GetName() == name)
            return scene.get();
    }
    return nullptr;
}

} // namespace Leir
