// Grid.frag.wgsl - color + 1px anti-aliased edges + PER-PIXEL DISTANCE FADE
// (fog by depth) for the editor ground grid (WebGPU backend). Mirrors
// Grid.frag.slang: the constant-pixel-width quad shape comes from
// Grid.vert.wgsl; this shader applies the AA ramp from the perpendicular
// offset (sidePx) AND the Unity-style distance fade from the interpolated view
// depth (depth). spacing <= 0 (opaque origin axes) skips the distance fade.

struct PSInput {
    @location(0) fragColor: vec4<f32>,
    @location(1) sidePx: f32,
    @location(2) widthPx: f32,
    // View depth (clip.w) interpolated across the quad (see Grid.vert.wgsl).
    @location(3) depth: f32,
    // Level spacing (0 = opaque line: skip the distance fade).
    @location(4) spacing: f32,
};

// Shared with Grid.vert.wgsl; must match the C++ GridPushConstants (32 bytes).
struct PushConstants {
    viewportWidth: f32,
    viewportHeight: f32,
    scale: f32,            // px per unit at depth 1 (vh * 0.5 * f)
    fadeStart: f32,        // cell-size fade band (px of cell)
    fadeEnd: f32,
    horizonStart: f32,     // horizon fade band (view depth, units)
    horizonEnd: f32,
    overrideDensity: f32,  // >=0: manual mode (uniform density)
};

@group(1) @binding(0) var<uniform> push: PushConstants;

@fragment
fn ps_main(input: PSInput) -> @location(0) vec4<f32> {
    // Fully opaque core = width/2 - 0.5px from the centerline, then a 1px
    // fade (smoothstep) to the quad edge (width/2 + 0.5px).
    let halfCore = max(input.widthPx * 0.5 - 0.5, 0.0);
    var a: f32;
    if (input.widthPx < 1.0) {
        // Sub-pixel line: the VS quad is clamped to 1px (no rasterization
        // gaps), so sidePx spans [-0.5, 0.5]. Scale the 1px fade by the REAL
        // width so a 0.5px line still renders visibly THINNER than 1px.
        a = input.widthPx * (1.0 - smoothstep(0.0, 1.0, abs(input.sidePx)));
    } else {
        a = 1.0 - smoothstep(halfCore, halfCore + 1.0, abs(input.sidePx));
    }

    // Per-pixel distance fade (fog by depth): the line dissolves as the view
    // depth grows. fadeCell (cell-size, Unity-style) + fadeHorizon, min()ed.
    // spacing <= 0 (origin axes) skips the fade entirely; manual mode
    // (overrideDensity >= 0) uses the uniform override density and skips the
    // horizon fade (it is a pure LOD transition test, camera-independent).
    let depthC = max(input.depth, 0.1); // near-plane guard
    var fade: f32 = 1.0;
    if (input.spacing > 0.0) {
        let density = select(push.scale / depthC, push.overrideDensity,
                             push.overrideDensity >= 0.0);
        let cellPx = input.spacing * density;
        let fadeCell = smoothstep(push.fadeStart, push.fadeEnd, cellPx);
        let fadeHorizon = select(1.0 - smoothstep(push.horizonStart,
                                                  push.horizonEnd, depthC),
                                 1.0, push.overrideDensity >= 0.0);
        fade = min(fadeCell, fadeHorizon);
    }
    return vec4<f32>(input.fragColor.rgb, input.fragColor.a * a * fade);
}