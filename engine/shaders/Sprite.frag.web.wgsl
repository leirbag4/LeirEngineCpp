// Sprite.frag.web.wgsl - browser variant of Sprite.frag.wgsl. naga's web build
// cannot compile binding_array (the wgpu_binding_array enable is native-only),
// so this variant uses a single texture/sampler pair at group 0 that the
// backend binds per draw from the draw's sampled texture.

struct PSInput {
    @location(0) fragTexCoord: vec2<f32>,
};

struct SpritePushConstants {
    mvp: mat4x4<f32>,
    color: vec4<f32>,
    uvRect: vec4<f32>,
    textureIndex: u32,
};

@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;
@group(1) @binding(0) var<uniform> push: SpritePushConstants;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let uv = push.uvRect.xy + in.fragTexCoord * push.uvRect.zw;
    return textureSample(tex, samp, uv) * push.color;
}