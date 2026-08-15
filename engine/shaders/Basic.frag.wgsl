// Basic.frag.wgsl - 3D mesh fragment shader (Fase 5, WebGPU backend).
// Bindless texture table at group 1 (binding 0 = views, binding 1 = samplers).

struct PSInput {
    @location(0) fragNormal: vec3<f32>,
    @location(1) fragTexCoord: vec2<f32>,
    @location(2) fragWorldPos: vec3<f32>,
};

// Same layout as the vertex stage (group 2).
struct PushConstants {
    lightDir: vec3<f32>,
    lightColor: vec3<f32>,
    ambientColor: vec3<f32>,
    color: vec4<f32>,
    model: mat4x4<f32>,
    textureIndex: u32,
};

@group(1) @binding(0) var textures: binding_array<texture_2d<f32>, 16>;
@group(1) @binding(1) var samplers: binding_array<sampler, 16>;
@group(2) @binding(0) var<uniform> push: PushConstants;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let normal = normalize(in.fragNormal);
    let lightDir = normalize(push.lightDir);

    let diff = max(dot(normal, -lightDir), 0.0);
    let diffuse = push.lightColor * diff;

    let ambient = push.ambientColor;

    // Index from the push constant: uniform across the draw.
    let texColor = textureSample(textures[push.textureIndex], samplers[push.textureIndex], in.fragTexCoord);
    let baseColor = texColor * push.color;

    return vec4<f32>((ambient + diffuse) * baseColor.rgb, baseColor.a);
}