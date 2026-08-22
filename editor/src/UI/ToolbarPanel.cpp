#include "ToolbarPanel.h"
#include "LeirEngine/UI/Font.h"

ToolbarPanel::ToolbarPanel()
{
    SetName("ToolbarPanel");
    SetColor({0.12f, 0.12f, 0.14f, 1.0f});
    SetLayoutMode(Leir::LayoutMode::Row);
    SetPadding(8.0f, 4.0f, 8.0f, 4.0f);
    SetSpacing(6.0f);

    auto makeButton = [this](const std::string& text) {
        auto* b = new Leir::UIButton();
        b->SetText(text);
        b->SetSizePolicy(Leir::SizePolicy::Fixed);
        b->SetColors({0.22f, 0.22f, 0.26f, 1.0f},
                     {0.32f, 0.32f, 0.38f, 1.0f},
                     {0.16f, 0.16f, 0.2f, 1.0f});
        b->SetTextColor({0.85f, 0.85f, 0.9f, 1.0f});
        AddChild(b);
        return b;
    };

    m_ButtonW = makeButton("W");
    m_ButtonE = makeButton("E");
    m_ButtonR = makeButton("R");

    // Thin separator label.
    m_Spacer = new Leir::UILabel();
    m_Spacer->SetText("|");
    m_Spacer->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_Spacer->SetColor({0.4f, 0.4f, 0.45f, 1.0f});
    AddChild(m_Spacer);

    m_SpaceButton = new Leir::UIButton();
    m_SpaceButton->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_SpaceButton->SetColors({0.22f, 0.22f, 0.26f, 1.0f},
                             {0.32f, 0.32f, 0.38f, 1.0f},
                             {0.16f, 0.16f, 0.2f, 1.0f});
    m_SpaceButton->SetTextColor({0.85f, 0.85f, 0.9f, 1.0f});
    AddChild(m_SpaceButton);

    m_ButtonW->SetOnClick([this]() {
        if (m_Tool != Tool::Translate) {
            m_Tool = Tool::Translate;
            ApplyToolState();
            if (m_OnToolChanged) m_OnToolChanged(m_Tool);
        }
    });
    m_ButtonE->SetOnClick([this]() {
        if (m_Tool != Tool::Rotate) {
            m_Tool = Tool::Rotate;
            ApplyToolState();
            if (m_OnToolChanged) m_OnToolChanged(m_Tool);
        }
    });
    m_ButtonR->SetOnClick([this]() {
        if (m_Tool != Tool::Scale) {
            m_Tool = Tool::Scale;
            ApplyToolState();
            if (m_OnToolChanged) m_OnToolChanged(m_Tool);
        }
    });
    m_SpaceButton->SetOnClick([this]() {
        if (m_ScaleMode)
            return; // scale is always local
        m_Space = (m_Space == Space::Global) ? Space::Local : Space::Global;
        ApplySpaceState();
        if (m_OnSpaceChanged) m_OnSpaceChanged(m_Space);
    });

    ApplyToolState();
    ApplySpaceState();
}

ToolbarPanel::~ToolbarPanel() = default;

void ToolbarPanel::SetFont(Leir::Font* font)
{
    for (auto* child : GetChildren()) {
        if (auto* b = dynamic_cast<Leir::UIButton*>(child)) {
            b->SetFont(font);
        } else if (auto* l = dynamic_cast<Leir::UILabel*>(child)) {
            l->SetFont(font);
        }
    }
}

void ToolbarPanel::SetTool(Tool t)
{
    if (m_Tool == t)
        return;
    m_Tool = t;
    ApplyToolState();
}

void ToolbarPanel::SetSpace(Space s)
{
    if (m_Space == s)
        return;
    m_Space = s;
    ApplySpaceState();
}

void ToolbarPanel::SetScaleMode(bool scale)
{
    if (m_ScaleMode == scale)
        return;
    m_ScaleMode = scale;
    ApplySpaceState();
}

Leir::Vector2 ToolbarPanel::GetMinSize() const
{
    return {0.0f, 30.0f};
}

void ToolbarPanel::OnLayoutComputed()
{
    Leir::UIPanel::OnLayoutComputed();
}

void ToolbarPanel::ApplyToolState()
{
    // The active tool button is highlighted (lighter), the rest dim.
    struct Spec { Leir::UIButton* b; Tool t; };
    Spec specs[3] = {
        { m_ButtonW, Tool::Translate },
        { m_ButtonE, Tool::Rotate },
        { m_ButtonR, Tool::Scale },
    };
    for (const auto& s : specs) {
        if (!s.b)
            continue;
        const bool active = (s.t == m_Tool);
        s.b->SetColors(
            active ? Leir::Vector4(0.45f, 0.45f, 0.55f, 1.0f)
                   : Leir::Vector4(0.22f, 0.22f, 0.26f, 1.0f),
            active ? Leir::Vector4(0.55f, 0.55f, 0.65f, 1.0f)
                   : Leir::Vector4(0.32f, 0.32f, 0.38f, 1.0f),
            active ? Leir::Vector4(0.35f, 0.35f, 0.45f, 1.0f)
                   : Leir::Vector4(0.16f, 0.16f, 0.2f, 1.0f));
        s.b->SetTextColor(active ? Leir::Vector4(1.0f, 1.0f, 1.0f, 1.0f)
                                 : Leir::Vector4(0.85f, 0.85f, 0.9f, 1.0f));
    }
}

void ToolbarPanel::ApplySpaceState()
{
    if (!m_SpaceButton)
        return;
    m_SpaceButton->SetText(m_ScaleMode ? "Scale" : (m_Space == Space::Global ? "Global" : "Local"));
    if (m_ScaleMode) {
        // Grayed out: scale has a single mode.
        m_SpaceButton->SetColors({0.16f, 0.16f, 0.18f, 1.0f},
                                 {0.16f, 0.16f, 0.18f, 1.0f},
                                 {0.16f, 0.16f, 0.18f, 1.0f});
        m_SpaceButton->SetTextColor({0.45f, 0.45f, 0.48f, 1.0f});
    } else {
        const bool active = (m_Space == Space::Global);
        m_SpaceButton->SetTextColor({0.9f, 0.9f, 0.95f, 1.0f});
        // Both Global/Local are interactive; highlight the active one lightly.
        m_SpaceButton->SetColors(
            active ? Leir::Vector4(0.38f, 0.38f, 0.48f, 1.0f)
                   : Leir::Vector4(0.22f, 0.22f, 0.26f, 1.0f),
            active ? Leir::Vector4(0.48f, 0.48f, 0.58f, 1.0f)
                   : Leir::Vector4(0.32f, 0.32f, 0.38f, 1.0f),
            active ? Leir::Vector4(0.3f, 0.3f, 0.4f, 1.0f)
                   : Leir::Vector4(0.16f, 0.16f, 0.2f, 1.0f));
    }
}