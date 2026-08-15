// UI.frag.wgsl - UI quad fragment shader (Fase 5, WebGPU backend).
// Bindless texture table at group 0 (binding 0 = views, binding 1 = samplers).
// Non-uniform resource indexing is implicit in WGSL (the equivalent of
// NonUniformResourceIndex is not needed).

struct PSInput {
    @location(0) fragTexCoord: vec2<f32>,
    @location(1) fragColor: vec4<f32>,
    @location(2) fragTexIndex: f32,
};

@group(0) @binding(0) var textures: binding_array<texture_2d<f32>, 16>;
@group(0) @binding(1) var samplers: binding_array<sampler, 16>;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let idx = u32(in.fragTexIndex);
    let texColor = textureSample(textures[idx], samplers[idx], in.fragTexCoord);
    return texColor * in.fragColor;
}