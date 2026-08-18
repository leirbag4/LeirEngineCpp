// Grid.frag.wgsl - editor ground grid fragment shader (WebGPU backend).
// Mirrors Grid.frag.slang: procedural grid lines computed per-fragment from the
// distance to the nearest line, converted to screen pixels with fwidth() so the
// line width is constant on screen at any distance/angle (anti-aliased 1px
// ramp). LOD fade by horizontal camera distance; chunk lines (every 10*unit)
// slightly brighter; mode=1 draws the origin axes (red X / blue Z).

struct PSInput {
    @location(0) worldPos: vec3<f32>,
};

struct PushConstants {
    unit: f32,
    fadeStart: f32,
    fadeEnd: f32,
    lineWidth: f32,
    mode: f32,
    pad1: f32,
    pad2: f32,
    pad3: f32,
    cameraPos: vec3<f32>,
    pad4: f32,
    baseColor: vec4<f32>,
    chunkColor: vec4<f32>,
};

// NOTE: no UBO here — the set-0 uniform is vertex-stage only on D3D12 (the
// backend root signature binds it with VERTEX visibility), so the fragment
// shader must never read it. cameraPos lives in the push constants.

@group(1) @binding(0) var<uniform> push: PushConstants;

// Distance to the nearest line of the given spacing in screen pixels (0 on the
// line), anti-aliased to an alpha-coverage ramp of `width` pixels.
fn gridLineAlpha(worldXZ: vec2<f32>, spacing: f32) -> f32 {
    let p = worldXZ / spacing;
    let dist = abs(fract(p - 0.5) - 0.5) / fwidth(p);
    return 1.0 - min(min(dist.x, dist.y) / push.lineWidth, 1.0);
}

@fragment
fn ps_main(input: PSInput) -> @location(0) vec4<f32> {
    var fade: f32 = 1.0;
    if (push.fadeEnd > push.fadeStart) {
        let d = distance(push.cameraPos.xz, input.worldPos.xz);
        fade = 1.0 - smoothstep(push.fadeStart, push.fadeEnd, d);
    }

    var alpha: f32;
    var color: vec3<f32>;
    if (push.mode > 0.5) {
        let ax = abs(input.worldPos.x) / fwidth(input.worldPos.x);
        let az = abs(input.worldPos.z) / fwidth(input.worldPos.z);
        let axisX = 1.0 - min(az / push.lineWidth, 1.0);
        let axisZ = 1.0 - min(ax / push.lineWidth, 1.0);
        color = mix(push.chunkColor.rgb, push.baseColor.rgb, axisX);
        alpha = max(axisX, axisZ);
    } else {
        let xz = input.worldPos.xz;
        let line = gridLineAlpha(xz, push.unit);
        let chunk = gridLineAlpha(xz, push.unit * 10.0);
        color = mix(push.baseColor.rgb, push.chunkColor.rgb, chunk);
        alpha = line;
    }

    return vec4<f32>(color * fade, alpha * fade);
}
