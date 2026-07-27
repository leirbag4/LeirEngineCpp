#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Scene/Scene.h"

#include <string>
#include <memory>
#include <vector>

namespace Leir {

class LEIR_API SceneManager {
public:
    static SceneManager& GetInstance();

    Scene& CreateScene(const std::string& name = "Untitled Scene");
    void DestroyScene(Scene* scene);
    void DestroyScene(const std::string& name);

    Scene* GetActiveScene() const { return m_ActiveScene; }
    void SetActiveScene(Scene* scene);
    void SetActiveScene(const std::string& name);

    Scene* FindScene(const std::string& name) const;

private:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    std::vector<std::unique_ptr<Scene>> m_Scenes;
    Scene* m_ActiveScene = nullptr;
};

} // namespace Leir
