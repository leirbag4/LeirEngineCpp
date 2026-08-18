// Gizmo.vert.wgsl - procedural constant-pixel-width 3D line vertex shader
// (WebGPU backend). Mirrors Gizmo.vert.slang: each line is a 4-corner triangle
// strip; each corner carries the full segment + corner selectors. The shader
// projects both endpoints, takes the screen-space (pixel) perpendicular and
// expands by width/2 px, giving a perspective-correct constant-pixel-width
// line.

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) fragColor: vec4<f32>,
    // Signed perpendicular offset from the centerline in PIXELS, interpolated
    // across the quad. The fragment shader uses it for the 1px AA ramp.
    @location(1) sidePx: f32,
    // Line width in pixels, interpolated (per-line, uniform across the quad).
    @location(2) widthPx: f32,
};

struct UniformBufferObject {
    viewProjection: mat4x4<f32>,
};

struct PushConstants {
    viewportWidth: f32,
    viewportHeight: f32,
    pad0: f32,
    pad1: f32,
};

@group(0) @binding(0) var<uniform> ubo: UniformBufferObject;
@group(1) @binding(0) var<uniform> push: PushConstants;

@vertex
fn vs_main(
    @location(0) start: vec3<f32>,
    @location(1) end: vec3<f32>,
    @location(2) color: vec4<f32>,
    @location(3) cornerX: f32,
    @location(4) cornerY: f32,
    @location(5) width: f32,
) -> VSOutput {
    var out: VSOutput;

    let clipS = ubo.viewProjection * vec4<f32>(start, 1.0);
    let clipE = ubo.viewProjection * vec4<f32>(end, 1.0);
    let ndcS = clipS.xy / clipS.w;
    let ndcE = clipE.xy / clipE.w;

    let vw = max(push.viewportWidth, 1.0);
    let vh = max(push.viewportHeight, 1.0);

    let t = cornerX;
    let w = mix(clipS.w, clipE.w, t);
    let z = mix(clipS.z, clipE.z, t);

    // Line direction in screen pixels (isotropic), then a unit perpendicular.
    let dirPx = (ndcE - ndcS) * vec2<f32>(0.5 * vw, 0.5 * vh);
    let perp = vec2<f32>(-dirPx.y, dirPx.x);
    let dirLen = length(perp);
    var perpN = vec2<f32>(0.0, 0.0);
    if (dirLen > 1e-6) {
        perpN = perp / dirLen;
    }

    // Expand by width/2 pixels along the perpendicular and convert back to NDC.
    let offsetPx = perpN * (width * 0.5 * cornerY);
    let offsetNdc = offsetPx / vec2<f32>(0.5 * vw, 0.5 * vh);

    let ndc = mix(ndcS, ndcE, t) + offsetNdc;
    out.position = vec4<f32>(ndc * w, z, w);
    out.fragColor = color;
    out.sidePx = width * 0.5 * cornerY;
    out.widthPx = width;
    return out;
}