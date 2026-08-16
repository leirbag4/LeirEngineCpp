// Basic.frag.web.wgsl - browser variant of Basic.frag.wgsl. naga's web build
// cannot compile binding_array (the wgpu_binding_array enable is native-only),
// so this variant uses a single texture/sampler pair at group 1 that the
// backend binds per draw from the draw's sampled texture.

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

@group(1) @binding(0) var tex: texture_2d<f32>;
@group(1) @binding(1) var samp: sampler;
@group(2) @binding(0) var<uniform> push: PushConstants;

@fragment
fn ps_main(in: PSInput) -> @location(0) vec4<f32> {
    let normal = normalize(in.fragNormal);
    let lightDir = normalize(push.lightDir);

    let diff = max(dot(normal, -lightDir), 0.0);
    let diffuse = push.lightColor * diff;

    let ambient = push.ambientColor;

    let texColor = textureSample(tex, samp, in.fragTexCoord);
    let baseColor = texColor * push.color;

    return vec4<f32>((ambient + diffuse) * baseColor.rgb, baseColor.a);
}