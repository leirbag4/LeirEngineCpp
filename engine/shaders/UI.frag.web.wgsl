// UI.frag.web.wgsl - browser variant of UI.frag.wgsl. naga's web build cannot
// compile binding_array (the wgpu_binding_array enable is native-only), so this
// variant uses a single texture/sampler pair at group 0 that the backend binds
// per draw from the draw's sampled texture.

struct PSInput {
    @location(0) fragTexCoord: vec2<f32>,
    @location(1) fragColor: vec4<f32>,
};

@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let texColor = textureSample(tex, samp, in.fragTexCoord);
    return texColor * in.fragColor;
}