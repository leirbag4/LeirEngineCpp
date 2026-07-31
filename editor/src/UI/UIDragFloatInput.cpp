#include "UIDragFloatInput.h"
#include <spdlog/spdlog.h>

UIDragFloatInput::UIDragFloatInput()
{
    SetColor({0.15f, 0.15f, 0.17f, 1.0f});
    SetLayoutMode(Leir::LayoutMode::Row);
    SetSpacing(4.0f);
    SetPadding(4.0f, 2.0f, 4.0f, 2.0f);

    m_Label = new Leir::UILabel();
    m_Label->SetName("DragLabel");
    m_Label->SetText("X:");
    m_Label->SetColor({0.8f, 0.8f, 0.8f, 1.0f});
    m_Label->SetFontSize(14);
    m_Label->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_Label);

    m_Input = new Leir::UIFloatInput();
    m_Input->SetName("DragInput");
    m_Input->SetValue(0.0f);
    m_Input->SetSizePolicy(Leir::SizePolicy::Fill);
    m_Input->SetAutoSelect(true);
    AddChild(m_Input);
}

UIDragFloatInput::~UIDragFloatInput() = default;

void UIDragFloatInput::SetLabel(const std::string& text)
{
    if (m_Label)
        m_Label->SetText(text);
}

void UIDragFloatInput::SetValue(float v)
{
    if (m_Input)
        m_Input->SetValue(v);
}

float UIDragFloatInput::GetValue() const
{
    return m_Input ? m_Input->GetValue() : 0.0f;
}

void UIDragFloatInput::SetFont(Leir::Font* font)
{
    if (m_Label) m_Label->SetFont(font);
    if (m_Input) m_Input->SetFont(font);
}

bool UIDragFloatInput::OnPointerDown(const Leir::Vector2& pos)
{
    if (!m_Label) return false;

    const auto& lr = m_Label->GetComputedRect();
    bool onLabel = pos.x >= lr.x && pos.x <= lr.x + lr.z &&
                   pos.y >= lr.y && pos.y <= lr.y + lr.w;

    spdlog::trace("[DragInput '{}'] OnPointerDown pos=({:.1f},{:.1f}) labelRect=({:.1f},{:.1f},{:.1f},{:.1f}) onLabel={}",
        m_Label->GetText().c_str(), pos.x, pos.y, lr.x, lr.y, lr.z, lr.w, onLabel);

    if (!onLabel) return false;

    m_Dragging = true;
    m_DragStartX = pos.x;
    m_DragStartValue = GetValue();

    Leir::UIElement* e = this;
    while (e) {
        auto* c = dynamic_cast<Leir::UICanvas*>(e);
        if (c) {
            spdlog::trace("[DragInput] Capturing pointer for drag");
            c->CapturePointer(this);
            break;
        }
        e = e->GetParent();
    }

    return true;
}

void UIDragFloatInput::OnPointerMove(const Leir::Vector2& pos)
{
    if (!m_Dragging) return;

    float delta = (pos.x - m_DragStartX) * 0.01f;
    float newVal = m_DragStartValue + delta;
    spdlog::trace("[DragInput] DragMove: dragStartX={:.1f} currentX={:.1f} delta={:.4f} value={:.3f}",
        m_DragStartX, pos.x, delta, newVal);
    SetValue(newVal);
    if (m_OnValueChanged)
        m_OnValueChanged(newVal);
}

bool UIDragFloatInput::OnPointerUp(const Leir::Vector2& pos)
{
    if (!m_Dragging) return false;
    spdlog::trace("[DragInput] DragEnd");
    m_Dragging = false;
    return true;
}

Leir::Vector2 UIDragFloatInput::GetMinSize() const
{
    return {60.0f, 22.0f};
}

void UIDragFloatInput::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}
