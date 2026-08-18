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
    let a = 1.0 - smoothstep(halfCore, halfCore + 1.0, abs(input.sidePx));
    return vec4<f32>(input.fragColor.rgb, input.fragColor.a * a);
}
