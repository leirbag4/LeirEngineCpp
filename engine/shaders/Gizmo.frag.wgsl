// Gizmo.frag.wgsl - pass-through color for the gizmo line renderer (WebGPU
// backend). Mirrors Gizmo.frag.slang.

struct PSInput {
    @location(0) fragColor: vec4<f32>,
};

@fragment
fn ps_main(input: PSInput) -> @location(0) vec4<f32> {
    return input.fragColor;
}
