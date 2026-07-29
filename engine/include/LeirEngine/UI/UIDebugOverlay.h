#pragma once
#include "LeirEngine/Core/Export.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leir {

class Font;
class UICanvas;

class LEIR_API UIDebugOverlay {
public:
    UIDebugOverlay(Font* font, UICanvas* canvas);
    ~UIDebugOverlay();

    void Update(float deltaTime);
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

private:
    void CreatePanel(Font* font);

    UICanvas* m_Canvas = nullptr;

    class UIPanel* m_Panel = nullptr;
    class UILabel* m_FpsLabel = nullptr;
    class UILabel* m_MouseLabel = nullptr;
    class UILabel* m_ButtonsLabel = nullptr;
    class UILabel* m_KeysLabel = nullptr;
    class UILabel* m_HoverLabel = nullptr;
    class UILabel* m_LastEventLabel = nullptr;

    bool m_Active = true;

    // FPS smoothing
    float m_FpsAccum = 0.0f;
    int m_FrameCount = 0;
    float m_CurrentFps = 0.0f;

    // Event tracking
    class UIElement* m_LastHovered = nullptr;
    std::string m_LastEvent;
    int m_LastEventFrames = 0;
};

} // namespace Leir
