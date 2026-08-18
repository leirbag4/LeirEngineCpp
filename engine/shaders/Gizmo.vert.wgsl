// Gizmo.vert.wgsl - procedural constant-pixel-width 3D line vertex shader
// (WebGPU backend). Mirrors Gizmo.vert.slang: each line is a 4-corner triangle
// strip; each corner carries the full segment + corner selectors. The shader
// projects both endpoints, takes the screen-space perpendicular and expands by
// width/2 px, giving a perspective-correct constant-pixel-width line.

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) fragColor: vec4<f32>,
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

    let scale = vec2<f32>(push.viewportHeight / push.viewportWidth, 1.0);
    let invScale = vec2<f32>(push.viewportWidth / push.viewportHeight, 1.0);
    let sS = ndcS * scale;
    let sE = ndcE * scale;

    let t = cornerX;
    let w = mix(clipS.w, clipE.w, t);
    let z = mix(clipS.z, clipE.z, t);

    let dir = sE - sS;
    var perp = vec2<f32>(0.0, 0.0);
    if (length(dir) > 1e-6) {
        perp = normalize(vec2<f32>(-dir.y, dir.x));
    }

    let expanded = mix(sS, sE, t) + perp * (width * 0.5 * cornerY);
    out.position = vec4<f32>(expanded * invScale * w, z, w);
    out.fragColor = color;
    return out;
}
