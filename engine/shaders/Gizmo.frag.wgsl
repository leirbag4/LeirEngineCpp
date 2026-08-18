// Gizmo.frag.wgsl - color + 1px anti-aliased edges for the gizmo line renderer
// (WebGPU backend). Mirrors Gizmo.frag.slang.

struct PSInput {
    @location(0) fragColor: vec4<f32>,
    @location(1) sidePx: f32,
    @location(2) widthPx: f32,
};

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
    return vec4<f32>(input.fragColor.rgb, input.fragColor.a * a);
}
