#include "LeirEngine/UI/UIWindowInternal.h"
#include "LeirEngine/UI/UICanvas.h"

namespace Leir {

UIWindowInternal::UIWindowInternal(const std::string& title)
    : UIWindow(title)
{
    SetName("UIWindowInternal");
}

UIWindowInternal::~UIWindowInternal() = default;

void UIWindowInternal::ShowIn(UICanvas* canvas)
{
    if (m_Visible)
        return;
    m_ParentWindow = nullptr;
    m_Modal = false;

    if (!GetParent() && canvas)
        canvas->AddChild(this);

    Activate();
}

void UIWindowInternal::ShowModalIn(UICanvas* canvas)
{
    if (m_Visible)
        return;
    m_ParentWindow = nullptr;
    m_Modal = true;

    if (!GetParent() && canvas) {
        // Semi-transparent overlay that blocks clicks to the parent canvas.
        // The overlay is added BEFORE the window so the window renders on top.
        m_ModalOverlay = new UIPanel();
        m_ModalOverlay->SetName("ModalOverlay");
        m_ModalOverlay->SetColor({0.0f, 0.0f, 0.0f, 0.55f});
        m_ModalOverlay->GetRect().anchor = AnchorSet::Stretch();
        m_ModalOverlay->SetHitTestable(true);
        canvas->AddChild(m_ModalOverlay);
        canvas->AddChild(this);
    }

    Activate();
}

} // namespace Leir