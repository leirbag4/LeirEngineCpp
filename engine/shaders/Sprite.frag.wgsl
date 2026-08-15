// Sprite.frag.wgsl - 2D sprite fragment shader (Fase 5, WebGPU backend).
// Bindless texture table at group 0 (binding 0 = views, binding 1 = samplers).

struct PSInput {
    @location(0) fragTexCoord: vec2<f32>,
};

struct SpritePushConstants {
    mvp: mat4x4<f32>,
    color: vec4<f32>,
    uvRect: vec4<f32>,
    textureIndex: u32,
};

@group(0) @binding(0) var textures: binding_array<texture_2d<f32>, 16>;
@group(0) @binding(1) var samplers: binding_array<sampler, 16>;
@group(1) @binding(0) var<uniform> push: SpritePushConstants;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let uv = push.uvRect.xy + in.fragTexCoord * push.uvRect.zw;
    return textureSample(textures[push.textureIndex], samplers[push.textureIndex], uv) * push.color;
}