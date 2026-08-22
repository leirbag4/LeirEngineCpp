#pragma once

#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/Math/Quaternion.h"
#include "LeirEngine/Objects/Object3D.h"
#include "LeirEngine/Components/Camera.h"
#include "GizmoRenderer.h"

#include <functional>

// Unity-style transform gizmo (translate / rotate / scale) drawn in the editor
// viewport on the selected object, with constant screen size.
//
// Rotation NEVER touches Euler angles: every drag delta is applied as a
// quaternion multiplication (AngleAxis(delta, axis) * rot), so there is NO
// gimbal lock regardless of the object's orientation.
//
// Axis colors: X = red, Y = green, Z = blue (the same in all three tools);
// hovered handles lighten toward white.
class TransformGizmo {
public:
    enum class Tool { None, Translate, Rotate, Scale };
    enum class Space { Global, Local };

    // Per-frame input snapshot (editor fills it in OnUpdate).
    struct Frame {
        Leir::Camera* camera = nullptr;
        Leir::Vector4 viewportRect{ 0.0f, 0.0f, 1.0f, 1.0f }; // logical x,y,w,h
        float contentScale = 1.0f;                              // dpr
        Leir::Vector2 mousePos{ 0.0f, 0.0f };                   // logical screen px
        bool leftPressed = false;
        bool leftDown = false;
        bool leftReleased = false;
    };

    TransformGizmo();
    ~TransformGizmo();

    void SetTool(Tool t);
    Tool GetTool() const { return m_Tool; }
    void SetSpace(Space s);
    Space GetSpace() const { return m_Space; }
    // Scale is always local (single mode): the Global/Local toggle is disabled.
    bool IsScaleTool() const { return m_Tool == Tool::Scale; }

    void SetSelected(Leir::Object3D* obj);
    Leir::Object3D* GetSelected() const { return m_Selected; }

    // Handles hover + drag. Returns true if the frame's left press was consumed
    // by a gizmo handle (the caller must skip object selection/picking).
    bool Update(const Frame& frame);

    void Draw(GizmoRenderer& renderer, const Leir::Matrix4x4& viewProjection,
              float viewportWidthPx, float viewportHeightPx);

    bool IsDragging() const { return m_Drag.active; }
    bool IsOverHandle() const { return m_Hover != Handle::None; }

private:
    enum class Handle {
        None,
        AxisX, AxisY, AxisZ,
        PlaneX, PlaneY, PlaneZ, // plane squares (block the axis in the name)
        RingX, RingY, RingZ,
        CubeX, CubeY, CubeZ,    // scale handle cubes at the arrow tips
        Center,                 // uniform scale cube
    };

    struct Ray { Leir::Vector3 origin; Leir::Vector3 dir; };

    struct DragState {
        bool active = false;
        Handle handle = Handle::None;
        Leir::Vector3 startPos;        // object world pos at grab
        Leir::Quaternion startRot;     // object world rot at grab
        Leir::Vector3 startScale;      // object local scale at grab
        Leir::Vector3 axisDir;         // world axis direction of the handle
        Leir::Vector3 planeNormal;     // plane drag / rotate ring normal
        Leir::Vector3 basis0, basis1;  // orthonormal ring-plane basis
        float angle0 = 0.0f;           // rotate ring angle at grab
        Leir::Vector3 point0;          // ray-plane intersection at grab
        float t0 = 0.0f;               // axis parameter at grab
        float dist0 = 0.0f;            // center scale distance at grab
        Leir::Quaternion rotAccum;     // accumulated rotation during a ring drag
        Leir::Quaternion gizmoRot0;    // gizmo rotation at grab (rotate-global)
    };

    // World-space gizmo frame for the current tool/space.
    struct GizmoFrame {
        Leir::Vector3 center;
        Leir::Vector3 camPos;      // cached camera position (ring front arc)
        Leir::Vector3 axes[3]; // X,Y,Z world-space directions
        float worldPerPixel = 0.0f;
    };

    void ComputeFrame(const Frame& f, GizmoFrame& out) const;
    Ray BuildMouseRay(const Frame& f) const;
    Handle Pick(const Frame& f, const GizmoFrame& g, float& outScore) const;
    void BeginDrag(const Frame& f, const GizmoFrame& g, Handle h);
    void UpdateDrag(const Frame& f, const GizmoFrame& g);
    void EndDrag();

    bool RayPlane(const Ray& r, const Leir::Vector3& n, const Leir::Vector3& p,
                  Leir::Vector3& out) const;
    // Parameter t along the axis line (through `center`, direction `axis`) at
    // its closest approach to the mouse ray. Robust for any view angle (the
    // old ray-vs-plane axis drag degenerated when the camera sat IN the plane).
    bool ClosestPointOnAxis(const Ray& r, const Leir::Vector3& center,
                            const Leir::Vector3& axis, float& outT) const;
    float PointSegmentDistPx(const Leir::Vector2& p, const Leir::Vector2& a,
                             const Leir::Vector2& b) const;
    bool PointInQuadPx(const Leir::Vector2& p, const Leir::Vector2 quad[4]) const;

    // Draws the camera-facing half-arc of a ring (the "semi circumference").
    void DrawRingArc(GizmoRenderer& g, const Leir::Vector3& center,
                     float radius, const Leir::Vector3& normal,
                     const Leir::Vector4& color, const Leir::Vector3& camPos,
                     float widthPx);

    Tool m_Tool = Tool::Translate;
    Space m_Space = Space::Global;
    Leir::Object3D* m_Selected = nullptr;

    // Cached world frame from the last Update (used by Draw).
    GizmoFrame m_Frame;

    // Rotate-global orientation: accumulates drag deltas while the object stays
    // selected (rings keep their angle so you can continue rotating from there),
    // resets to identity on selection change. Local mode ignores it (rings
    // follow the object's world rotation each frame).
    Leir::Quaternion m_GizmoRotation;

    Handle m_Hover = Handle::None;
    DragState m_Drag;
};