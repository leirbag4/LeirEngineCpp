#include "TransformGizmo.h"

#include <LeirEngine/Core/Log.h>
#include <LeirEngine/Math/Mathf.h>
#include <LeirEngine/Math/Vector2.h>
#include <LeirEngine/Input/Mouse.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Gizmo dimensions in LOGICAL pixels (constant screen size at any zoom/dpi).
constexpr float kArrowLenPx = 80.0f;
constexpr float kArrowWidthPx = 2.5f;
constexpr float kConeLenPx = 18.0f;
constexpr float kConeRadiusPx = 8.0f;
constexpr float kPlaneSizePx = 34.0f;
constexpr float kPlaneAlpha = 0.25f;
constexpr float kRingRadiusPx = 55.0f;
constexpr float kRingWidthPx = 2.5f;
constexpr float kCubeHalfPx = 7.0f;
constexpr float kCenterCubeHalfPx = 9.0f;

// Pick thresholds in LOGICAL pixels.
constexpr float kAxisPickPx = 10.0f;
constexpr float kTipPickPx = 14.0f;
constexpr float kPlanePickPx = 8.0f;
constexpr float kRingPickPx = 9.0f;
constexpr float kCubePickPx = 13.0f;

// Axis colors (X red, Y green, Z blue) used identically in all three tools.
Leir::Vector4 AxisColor(int axis)
{
    switch (axis) {
        case 0: return { 1.0f, 0.25f, 0.25f, 1.0f };
        case 1: return { 0.25f, 1.0f, 0.25f, 1.0f };
        default: return { 0.25f, 0.5f, 1.0f, 1.0f };
    }
}

// Hovered handles lighten toward white.
Leir::Vector4 Hover(const Leir::Vector4& c)
{
    return c + (Leir::Vector4(1.0f, 1.0f, 1.0f, 1.0f) - c) * 0.55f;
}

// World-space axes for the gizmo: X, Y and +Z (Back). Forward() is -Z, so the
// Z arrow points toward +Z (blue) as in every 3D editor.
Leir::Vector3 WorldAxis(int i)
{
    switch (i) {
        case 0: return Leir::Vector3::Right();
        case 1: return Leir::Vector3::Up();
        default: return Leir::Vector3::Back();
    }
}

} // namespace

TransformGizmo::TransformGizmo() = default;
TransformGizmo::~TransformGizmo() = default;

void TransformGizmo::SetTool(Tool t)
{
    m_Tool = t;
    m_Hover = Handle::None;
}

void TransformGizmo::SetSpace(Space s)
{
    m_Space = s;
    m_Hover = Handle::None;
}

void TransformGizmo::SetSelected(Leir::Object3D* obj)
{
    if (m_Selected == obj)
        return;
    m_Selected = obj;
    // Reset the rotate-global orientation: rings come back to the initial
    // (axis-aligned) state on a new selection.
    m_GizmoRotation = Leir::Quaternion::Identity();
    m_Hover = Handle::None;
    m_Drag.active = false;
    m_Drag.handle = Handle::None;
}

bool TransformGizmo::Update(const Frame& frame)
{
    if (!m_Selected || !frame.camera)
        return false;
    if (m_Tool == Tool::None)
        return false;

    // Keep the view matrix in sync with the current camera transform (the
    // editor re-syncs the camera from EditorCamera every OnUpdate).
    frame.camera->RecalculateViewMatrix();

    GizmoFrame g;
    ComputeFrame(frame, g);
    m_Frame = g;

    if (m_Drag.active) {
        // Continue / finish an active drag (independent of viewport hover).
        if (frame.leftDown) {
            UpdateDrag(frame, g);
            m_Hover = m_Drag.handle;
        } else {
            EndDrag();
        }
        return false;
    }

    // Hover / press detection (screen space).
    m_Hover = Handle::None;
    float score = 0.0f;
    m_Hover = Pick(frame, g, score);

    if (frame.leftPressed && m_Hover != Handle::None) {
        BeginDrag(frame, g, m_Hover);
        return true; // consumed: caller must skip object selection
    }
    return false;
}

void TransformGizmo::ComputeFrame(const Frame& f, GizmoFrame& out) const
{
    out.center = m_Selected->GetTransform().GetWorldPosition();
    out.camPos = f.camera->GetOwner()->GetTransform().GetWorldPosition();

    // Effective gizmo orientation.
    Leir::Quaternion rot;
    if (m_Tool == Tool::Rotate) {
        rot = (m_Space == Space::Local) ? m_Selected->GetTransform().GetWorldRotation()
                                        : m_GizmoRotation;
    } else if (m_Tool == Tool::Scale || m_Space == Space::Local) {
        rot = m_Selected->GetTransform().GetWorldRotation();
    } else {
        rot = Leir::Quaternion::Identity();
    }
    for (int i = 0; i < 3; ++i)
        out.axes[i] = (rot * WorldAxis(i)).Normalized();

    // world-per-pixel at the gizmo center. Uses the LOGICAL viewport height so
    // the gizmo keeps a constant logical size (matches the mouse picking space).
    const Leir::Vector3 camPos = f.camera->GetOwner()->GetTransform().GetWorldPosition();
    const float dist = std::max((out.center - camPos).Length(), 1e-4f);
    const float fovRad = Leir::Mathf::Deg2Rad * f.camera->GetFOV();
    out.worldPerPixel = (2.0f * dist * std::tan(fovRad * 0.5f)) /
                        std::max(f.viewportRect.w, 1.0f);
}

TransformGizmo::Ray TransformGizmo::BuildMouseRay(const Frame& f) const
{
    const float vw = std::max(f.viewportRect.z, 1.0f);
    const float vh = std::max(f.viewportRect.w, 1.0f);
    const float ndcX = ((f.mousePos.x - f.viewportRect.x) / vw) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((f.mousePos.y - f.viewportRect.y) / vh) * 2.0f;
    const Leir::Matrix4x4 invVP = f.camera->GetViewProjectionMatrix().Inverse();
    auto unproj = [&](float z) -> Leir::Vector3 {
        const Leir::Vector4 c = invVP * Leir::Vector4(ndcX, ndcY, z, 1.0f);
        return Leir::Vector3(c.x / c.w, c.y / c.w, c.z / c.w);
    };
    const Leir::Vector3 nearP = unproj(-1.0f);
    const Leir::Vector3 farP = unproj(1.0f);
    Ray r;
    r.origin = nearP;
    r.dir = (farP - nearP).Normalized();
    return r;
}

bool TransformGizmo::RayPlane(const Ray& r, const Leir::Vector3& n,
                              const Leir::Vector3& p, Leir::Vector3& out) const
{
    const float denom = Leir::Vector3::Dot(r.dir, n);
    if (std::fabs(denom) < 1e-6f)
        return false;
    const float t = Leir::Vector3::Dot(p - r.origin, n) / denom;
    if (t < 0.0f)
        return false;
    out = r.origin + r.dir * t;
    return true;
}

bool TransformGizmo::ClosestPointOnAxis(const Ray& r, const Leir::Vector3& center,
                                        const Leir::Vector3& axis, float& outT) const
{
    // Closest point between the mouse ray (O + s*D) and the axis line
    // (C + t*A). Standard line-line closest point; t is the parameter along
    // the axis from `center`. Robust: never degenerates when the camera lies in
    // a plane containing the axis (the old ray-vs-plane method always hit the
    // ray origin there, so the axis drag never moved).
    const Leir::Vector3 w0 = r.origin - center;
    const float a = Leir::Vector3::Dot(r.dir, r.dir);
    const float b = Leir::Vector3::Dot(r.dir, axis);
    const float c = Leir::Vector3::Dot(axis, axis);
    const float d = Leir::Vector3::Dot(r.dir, w0);
    const float e = Leir::Vector3::Dot(axis, w0);
    const float denom = a * c - b * b;
    if (std::fabs(denom) < 1e-6f)
        return false; // ray ~parallel to the axis; caller falls back
    outT = (a * e - b * d) / denom;
    return true;
}

float TransformGizmo::PointSegmentDistPx(const Leir::Vector2& p,
                                         const Leir::Vector2& a,
                                         const Leir::Vector2& b) const
{
    const Leir::Vector2 ab = b - a;
    const float len2 = ab.SqrLength();
    if (len2 < 1e-8f)
        return (p - a).Length();
    const float t = Leir::Mathf::Clamp01(Leir::Vector2::Dot(p - a, ab) / len2);
    return (p - (a + ab * t)).Length();
}

bool TransformGizmo::PointInQuadPx(const Leir::Vector2& p,
                                   const Leir::Vector2 quad[4]) const
{
    for (int i = 0; i < 4; ++i) {
        const Leir::Vector2 e0 = quad[(i + 1) % 4] - quad[i];
        const Leir::Vector2 e1 = p - quad[i];
        if (Leir::Vector2::Cross(e0, e1) > 0.0f)
            return false;
    }
    return true;
}

TransformGizmo::Handle TransformGizmo::Pick(const Frame& f, const GizmoFrame& g,
                                            float& outScore) const
{
    // Project world -> LOGICAL viewport px (mouse space).
    const Leir::Matrix4x4 vp = f.camera->GetViewProjectionMatrix();
    const float vw = std::max(f.viewportRect.z, 1.0f);
    const float vh = std::max(f.viewportRect.w, 1.0f);
    // Mouse in viewport-local logical px (matches the projected handles).
    const Leir::Vector2 mouse(f.mousePos.x - f.viewportRect.x,
                              f.mousePos.y - f.viewportRect.y);
    auto toScreen = [&](const Leir::Vector3& w) -> Leir::Vector2 {
        const Leir::Vector4 clip = vp * Leir::Vector4(w.x, w.y, w.z, 1.0f);
        if (clip.w <= 1e-6f)
            return { -1e9f, -1e9f };
        const float nx = clip.x / clip.w;
        const float ny = clip.y / clip.w;
        return { (nx * 0.5f + 0.5f) * vw, (0.5f - ny * 0.5f) * vh };
    };

    Handle best = Handle::None;
    float bestScore = 1e9f;
    auto consider = [&](Handle h, float s) {
        if (s < bestScore) { bestScore = s; best = h; }
    };

    const Leir::Vector3 camPos = f.camera->GetOwner()->GetTransform().GetWorldPosition();

    if (m_Tool == Tool::Rotate) {
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 normal = g.axes[a];
            const float radius = kRingRadiusPx * g.worldPerPixel;
            Leir::Vector3 b0 = Leir::Vector3::Cross(normal,
                std::fabs(normal.y) < 0.9f ? Leir::Vector3::Up() : Leir::Vector3::Right());
            b0.Normalize();
            const Leir::Vector3 b1 = Leir::Vector3::Cross(normal, b0);
            const Leir::Vector3 D = (camPos - g.center).Normalized();
            const Leir::Vector3 Dp = D - normal * Leir::Vector3::Dot(D, normal);
            float ang0 = 0.0f;
            if (Dp.SqrLength() > 1e-6f)
                ang0 = std::atan2(Leir::Vector3::Dot(Dp, b1), Leir::Vector3::Dot(Dp, b0));
            const int kSeg = 24;
            Leir::Vector2 prevP = toScreen(g.center + (b0 * std::cos(ang0 - kPi * 0.5f) +
                                                       b1 * std::sin(ang0 - kPi * 0.5f)) * radius);
            float minDist = 1e9f;
            for (int i = 1; i <= kSeg; ++i) {
                const float aAng = ang0 - kPi * 0.5f + (float)i / (float)kSeg * kPi;
                const Leir::Vector2 curP = toScreen(g.center + (b0 * std::cos(aAng) +
                                                                b1 * std::sin(aAng)) * radius);
                minDist = std::min(minDist, PointSegmentDistPx(mouse, prevP, curP));
                prevP = curP;
            }
            if (minDist < kRingPickPx)
                consider(Handle((int)Handle::RingX + a), minDist);
        }
        outScore = bestScore;
        return best;
    }

    const float len = kArrowLenPx * g.worldPerPixel;
    const bool isScale = m_Tool == Tool::Scale;

    if (isScale) {
        // ---- Scale: the center cube takes priority over the arrow shafts
        // (they overlap near the origin, so checking the arrows first made the
        // center cube nearly impossible to grab / always showed an axis hover).
        const float dc = (mouse - toScreen(g.center)).Length();
        if (dc < kCubePickPx) {
            outScore = dc;
            return Handle::Center;
        }
    }

    if (!isScale) {
        // ---- Translate: the plane squares win over the arrow lines ----
        // When the camera-facing planes reorient, the perpendicular arrow shaft
        // projects inside the plane's quad with ~0px distance, so testing the
        // arrows first made them steal the hover (a distance tie). The planes
        // are tested FIRST, and when the cursor is inside one the NEAREST plane
        // to the camera wins (a tie across two planes -> pick the front one).
        // Geometry matches Draw: all three squares SHARE the gizmo-center
        // corner (no per-square normal offset -> they never cross) and extend
        // toward the camera in their two in-plane axes.
        const float s = kPlaneSizePx * g.worldPerPixel;
        const Leir::Vector3 camPosPlane =
            f.camera->GetOwner()->GetTransform().GetWorldPosition();
        const Leir::Vector3 camDirPlane = (camPosPlane - g.center).Normalized();
        const Ray pickRay = BuildMouseRay(f);
        Handle bestPlane = Handle::None;
        float bestPlaneDepth = 1e30f;
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 n = g.axes[a];
            const Leir::Vector3 u = g.axes[(a + 1) % 3];
            const Leir::Vector3 v = g.axes[(a + 2) % 3];
            const float su = Leir::Vector3::Dot(camDirPlane, u) >= 0.0f ? 1.0f : -1.0f;
            const float sv = Leir::Vector3::Dot(camDirPlane, v) >= 0.0f ? 1.0f : -1.0f;
            const Leir::Vector3 p0 = g.center; // shared corner
            const Leir::Vector3 p1 = p0 + u * (s * su);
            const Leir::Vector3 p2 = p0 + u * (s * su) + v * (s * sv);
            const Leir::Vector3 p3 = p0 + v * (s * sv);
            Leir::Vector2 quad[4];
            quad[0] = toScreen(p0);
            quad[1] = toScreen(p1);
            quad[2] = toScreen(p2);
            quad[3] = toScreen(p3);
            if (PointInQuadPx(mouse, quad)) {
                // Depth of the plane at the cursor (ray-plane hit distance to
                // the camera): the front-most plane wins ties.
                Leir::Vector3 hit;
                float depth = 1e30f;
                if (RayPlane(pickRay, n, g.center, hit))
                    depth = (hit - camPosPlane).Length();
                if (depth < bestPlaneDepth) {
                    bestPlaneDepth = depth;
                    bestPlane = Handle((int)Handle::PlaneX + a);
                }
            } else {
                float dq = 1e9f;
                for (int i = 0; i < 4; ++i)
                    dq = std::min(dq, PointSegmentDistPx(mouse, quad[i], quad[(i + 1) % 4]));
                if (dq < kPlanePickPx)
                    consider(Handle((int)Handle::PlaneX + a), dq);
            }
        }
        if (bestPlane != Handle::None) {
            outScore = 0.0f;
            return bestPlane;
        }
    }

    // Arrow shafts + tips (translate) / handle cubes (scale). Checked AFTER the
    // planes for translate: a plane the cursor is inside (score 0) always beats
    // the arrow line projected inside it.
    for (int a = 0; a < 3; ++a) {
        const Leir::Vector3 dir = g.axes[a];
        const Leir::Vector2 s0 = toScreen(g.center);
        const Leir::Vector2 s1 = toScreen(g.center + dir * len);
        const float dSeg = PointSegmentDistPx(mouse, s0, s1);
        if (dSeg < kAxisPickPx)
            consider(Handle((int)Handle::AxisX + a), dSeg);
        const float dTip = (mouse - s1).Length();
        if (dTip < kTipPickPx)
            consider(Handle((int)Handle::AxisX + a), dTip * 0.5f);
    }

    outScore = bestScore;
    return best;
}

void TransformGizmo::BeginDrag(const Frame& f, const GizmoFrame& g, Handle h)
{
    m_Drag.active = true;
    m_Drag.handle = h;
    m_Drag.startPos = m_Selected->GetTransform().GetWorldPosition();
    m_Drag.startRot = m_Selected->GetTransform().GetWorldRotation();
    m_Drag.startScale = m_Selected->GetTransform().GetLocalScale();
    m_Drag.rotAccum = m_Drag.startRot;
    m_Drag.gizmoRot0 = m_GizmoRotation;

    const Leir::Vector3 camPos = f.camera->GetOwner()->GetTransform().GetWorldPosition();
    const Leir::Vector3 viewDir = (camPos - g.center).Normalized();

    if (h >= Handle::AxisX && h <= Handle::AxisZ) {
        const int a = (int)h - (int)Handle::AxisX;
        m_Drag.axisDir = g.axes[a];
    } else if (h >= Handle::PlaneX && h <= Handle::PlaneZ) {
        m_Drag.planeNormal = g.axes[(int)h - (int)Handle::PlaneX]; // blocked axis
    } else if (h >= Handle::RingX && h <= Handle::RingZ) {
        const int a = (int)h - (int)Handle::RingX;
        m_Drag.axisDir = g.axes[a];
        Leir::Vector3 b0 = Leir::Vector3::Cross(m_Drag.axisDir,
            std::fabs(m_Drag.axisDir.y) < 0.9f ? Leir::Vector3::Up()
                                               : Leir::Vector3::Right());
        m_Drag.basis0 = b0.Normalized();
        m_Drag.basis1 = Leir::Vector3::Cross(m_Drag.axisDir, m_Drag.basis0).Normalized();
    } else if (h == Handle::Center) {
        m_Drag.planeNormal = viewDir;
    }

    // Grab intersection point / parameter for the drag. The anchor is the
    // object's START position (never the moving center): a plane anchored to
    // the live center drifts by float error along its blocked axis every frame.
    const Ray ray = BuildMouseRay(f);
    Leir::Vector3 hit;
    if (h >= Handle::PlaneX && h <= Handle::PlaneZ) {
        if (RayPlane(ray, m_Drag.planeNormal, m_Drag.startPos, hit))
            m_Drag.point0 = hit;
        else
            m_Drag.point0 = m_Drag.startPos;
    } else if (h == Handle::Center) {
        if (RayPlane(ray, m_Drag.planeNormal, m_Drag.startPos, hit))
            m_Drag.dist0 = std::max(1e-3f, (hit - m_Drag.startPos).Length());
        else
            m_Drag.dist0 = 1.0f;
    } else if (h >= Handle::AxisX && h <= Handle::AxisZ) {
        if (ClosestPointOnAxis(ray, m_Drag.startPos, m_Drag.axisDir, m_Drag.t0))
            ; // t0 = axis parameter at the grab point
        else
            m_Drag.t0 = 0.0f;
    } else if (h >= Handle::RingX && h <= Handle::RingZ) {
        if (RayPlane(ray, m_Drag.axisDir, m_Drag.startPos, hit)) {
            const Leir::Vector3 rel = hit - m_Drag.startPos;
            m_Drag.angle0 = std::atan2(Leir::Vector3::Dot(rel, m_Drag.basis1),
                                       Leir::Vector3::Dot(rel, m_Drag.basis0));
        } else {
            m_Drag.angle0 = 0.0f;
        }
    }
}

void TransformGizmo::UpdateDrag(const Frame& f, const GizmoFrame& g)
{
    if (!m_Selected)
        return;

    const Ray ray = BuildMouseRay(f);
    Leir::Vector3 hit;

    if (m_Drag.handle >= Handle::AxisX && m_Drag.handle <= Handle::AxisZ) {
        // ---- Translate / scale along an axis ----
        // Closest-point between the mouse ray and the axis line: robust for any
        // view angle (the camera never sits in the drag plane this way).
        const int a = (int)m_Drag.handle - (int)Handle::AxisX;
        float t = m_Drag.t0;
        if (!ClosestPointOnAxis(ray, m_Drag.startPos, m_Drag.axisDir, t))
            return;
        const float delta = t - m_Drag.t0;
        if (m_Tool == Tool::Scale) {
            const float factor = (m_Drag.t0 != 0.0f) ? t / m_Drag.t0 : 1.0f;
            Leir::Vector3 s = m_Drag.startScale;
            s[a] = std::max(0.001f, m_Drag.startScale[a] * factor);
            m_Selected->GetTransform().SetLocalScale(s);
        } else {
            m_Selected->GetTransform().SetWorldPosition(m_Drag.startPos + m_Drag.axisDir * delta);
        }
    } else if (m_Drag.handle >= Handle::PlaneX && m_Drag.handle <= Handle::PlaneZ) {
        // ---- Translate in a plane (blocking the perpendicular axis) ----
        if (!RayPlane(ray, m_Drag.planeNormal, m_Drag.startPos, hit))
            return;
        Leir::Vector3 delta = hit - m_Drag.point0;
        // Remove any float residue along the blocked axis so the object never
        // drifts on it (the green square must keep Y exactly at its start).
        delta = delta - m_Drag.planeNormal *
            Leir::Vector3::Dot(delta, m_Drag.planeNormal);
        m_Selected->GetTransform().SetWorldPosition(m_Drag.startPos + delta);
    } else if (m_Drag.handle >= Handle::RingX && m_Drag.handle <= Handle::RingZ) {
        // ---- Rotate around a ring's axis (quaternion only, no Euler) ----
        // The object rotation ACCUMULATES each frame's delta (AngleAxis(delta)
        // applied to the running rotation), never through Euler angles — so
        // there is no gimbal lock regardless of the orientation reached.
        const Leir::Vector3 n = m_Drag.axisDir;
        if (!RayPlane(ray, n, m_Drag.startPos, hit))
            return;
        const Leir::Vector3 rel = hit - m_Drag.startPos;
        const float angle = std::atan2(Leir::Vector3::Dot(rel, m_Drag.basis1),
                                       Leir::Vector3::Dot(rel, m_Drag.basis0));
        float delta = angle - m_Drag.angle0;
        if (delta > kPi) delta -= 2.0f * kPi;
        if (delta < -kPi) delta += 2.0f * kPi;
        m_Drag.angle0 = angle;
        const float deltaDeg = delta * Leir::Mathf::Rad2Deg;
        const Leir::Quaternion q = Leir::Quaternion::AngleAxis(deltaDeg, n);
        m_Drag.rotAccum = q * m_Drag.rotAccum;
        m_Selected->GetTransform().SetWorldRotation(m_Drag.rotAccum);
        if (m_Space == Space::Global)
            m_GizmoRotation = q * m_GizmoRotation;
    } else if (m_Drag.handle == Handle::Center) {
        // ---- Uniform scale ----
        if (!RayPlane(ray, m_Drag.planeNormal, m_Drag.startPos, hit))
            return;
        const float dist = std::max(1e-3f, (hit - m_Drag.startPos).Length());
        const float factor = dist / m_Drag.dist0;
        const Leir::Vector3 s = m_Drag.startScale * std::max(0.001f, factor);
        m_Selected->GetTransform().SetLocalScale(s);
    }
}

void TransformGizmo::EndDrag()
{
    m_Drag.active = false;
    m_Drag.handle = Handle::None;
    m_Hover = Handle::None;
}

void TransformGizmo::DrawRingArc(GizmoRenderer& g, const Leir::Vector3& center,
                                 float radius, const Leir::Vector3& normal,
                                 const Leir::Vector4& color, const Leir::Vector3& camPos,
                                 float widthPx)
{
    Leir::Vector3 b0 = Leir::Vector3::Cross(normal,
        std::fabs(normal.y) < 0.9f ? Leir::Vector3::Up() : Leir::Vector3::Right());
    b0.Normalize();
    const Leir::Vector3 b1 = Leir::Vector3::Cross(normal, b0);

    // Camera-facing half: angles centered on the projection of the camera
    // direction onto the ring plane (Unity style: the back half is not drawn).
    const Leir::Vector3 D = (camPos - center).Normalized();
    const Leir::Vector3 Dp = D - normal * Leir::Vector3::Dot(D, normal);
    float ang0 = 0.0f;
    if (Dp.SqrLength() > 1e-6f)
        ang0 = std::atan2(Leir::Vector3::Dot(Dp, b1), Leir::Vector3::Dot(Dp, b0));

    const int kSeg = 48;
    Leir::Vector3 prev = center + (b0 * std::cos(ang0 - kPi * 0.5f) +
                                   b1 * std::sin(ang0 - kPi * 0.5f)) * radius;
    for (int i = 1; i <= kSeg; ++i) {
        const float a = ang0 - kPi * 0.5f + (float)i / (float)kSeg * kPi;
        const Leir::Vector3 cur = center + (b0 * std::cos(a) + b1 * std::sin(a)) * radius;
        g.DrawLine(prev, cur, color, widthPx);
        prev = cur;
    }
}

void TransformGizmo::Draw(GizmoRenderer& g, const Leir::Matrix4x4& viewProjection,
                          float viewportWidthPx, float viewportHeightPx)
{
    (void)viewProjection;
    (void)viewportWidthPx;
    (void)viewportHeightPx;
    if (!m_Selected)
        return;
    if (m_Tool == Tool::None)
        return;

    const GizmoFrame gf = m_Frame;
    const float s = gf.worldPerPixel;
    const Leir::Vector3 center = gf.center;
    const float len = kArrowLenPx * s;
    const float coneLen = kConeLenPx * s;
    const float coneR = kConeRadiusPx * s;
    const float planeS = kPlaneSizePx * s;
    const float ringR = kRingRadiusPx * s;
    const float cubeHalf = kCubeHalfPx * s;
    const float centerHalf = kCenterCubeHalfPx * s;

    // Widths are in PHYSICAL px (the renderer expands them against the physical
    // viewport it was called with).
    const float wAxis = kArrowWidthPx;
    const float wRing = kRingWidthPx;

    const Leir::Vector3 camPos = m_Frame.camPos;

    auto colorFor = [&](int axis) -> Leir::Vector4 {
        Leir::Vector4 c = AxisColor(axis);
        // Highlight the hovered / dragged handle of this axis.
        bool isHover = false;
        if (IsDragging())
            isHover = (m_Drag.handle == Handle((int)Handle::AxisX + axis));
        else
            isHover = (m_Hover == Handle((int)Handle::AxisX + axis));
        if (isHover)
            c = Hover(c);
        return c;
    };

    if (m_Tool == Tool::Translate) {
        // ---- Axis arrows + cones ----
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 dir = gf.axes[a];
            const Leir::Vector3 tip = center + dir * len;
            g.DrawLine(center, tip, colorFor(a), wAxis);
            // Cone (arrow head).
            const Leir::Vector3 base = tip - dir * coneLen;
            Leir::Vector4 c = colorFor(a);
            c.w = 1.0f;
            g.DrawCone(base, coneR, tip, c);
        }
        // ---- Translucent plane squares (3-sided cube at the center) ----
        // Unity-style: the three squares SHARE a common corner at the gizmo
        // center (they interlock like a 3-sided cube and never cross each
        // other), and each extends toward the camera in its two in-plane axes
        // (su/sv signs) so the corner always faces the view.
        const Leir::Vector3 camDirPlane = (camPos - center).Normalized();
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 n = gf.axes[a];
            const Leir::Vector3 u = gf.axes[(a + 1) % 3];
            const Leir::Vector3 v = gf.axes[(a + 2) % 3];
            // Shared corner at the gizmo center (no per-square normal offset:
            // that made the squares cross each other near the origin).
            const Leir::Vector3 p0 = center;
            // Extend toward the camera in each in-plane axis.
            const float su = Leir::Vector3::Dot(camDirPlane, u) >= 0.0f ? 1.0f : -1.0f;
            const float sv = Leir::Vector3::Dot(camDirPlane, v) >= 0.0f ? 1.0f : -1.0f;
            const Leir::Vector3 p1 = p0 + u * (planeS * su);
            const Leir::Vector3 p2 = p0 + u * (planeS * su) + v * (planeS * sv);
            const Leir::Vector3 p3 = p0 + v * (planeS * sv);
            Leir::Vector4 c = AxisColor(a);
            bool isHover = IsDragging() ? (m_Drag.handle == Handle((int)Handle::PlaneX + a))
                                        : (m_Hover == Handle((int)Handle::PlaneX + a));
            if (isHover)
                c = Hover(c);
            c.w = kPlaneAlpha;
            g.DrawQuadFilled(p0, p1, p2, p3, c);
        }
    } else if (m_Tool == Tool::Rotate) {
        // ---- Rings (camera-facing half arc) ----
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 normal = gf.axes[a];
            Leir::Vector4 c = AxisColor(a);
            bool isHover = IsDragging() ? (m_Drag.handle == Handle((int)Handle::RingX + a))
                                        : (m_Hover == Handle((int)Handle::RingX + a));
            if (isHover)
                c = Hover(c);
            DrawRingArc(g, center, ringR, normal, c, camPos, wRing);
        }
    } else if (m_Tool == Tool::Scale) {
        // ---- Axis arrows + handle cubes, always in the object's local axes ----
        // The handle cubes are oriented by the object's rotation (same as the
        // translate cones): they rotate with the gizmo frame. Scale is always
        // local, so the cubes always follow the object's world rotation.
        const Leir::Quaternion cubeRot =
            m_Selected->GetTransform().GetWorldRotation();
        for (int a = 0; a < 3; ++a) {
            const Leir::Vector3 dir = gf.axes[a];
            const Leir::Vector3 tip = center + dir * len;
            g.DrawLine(center, tip, colorFor(a), wAxis);
            Leir::Vector4 c = colorFor(a);
            g.DrawCubeFilledOriented(tip,
                Leir::Vector3(cubeHalf * 2.0f, cubeHalf * 2.0f, cubeHalf * 2.0f),
                cubeRot, c);
        }
        // ---- Center cube (uniform scale), grayish; also oriented ----
        bool centerHover = IsDragging() ? (m_Drag.handle == Handle::Center)
                                        : (m_Hover == Handle::Center);
        Leir::Vector4 cc = centerHover ? Leir::Vector4(1.0f, 1.0f, 1.0f, 1.0f)
                                       : Leir::Vector4(0.7f, 0.7f, 0.7f, 1.0f);
        g.DrawCubeFilledOriented(center,
            Leir::Vector3(centerHalf * 2.0f, centerHalf * 2.0f, centerHalf * 2.0f),
            cubeRot, cc);
    }
}